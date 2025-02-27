/****************************************************************************
 *   Copyright (c) 2015 Eivind Kvedalen <eivind@kvedalen.name>              *
 *   Copyright (c) 2019 Zheng Lei (realthunder) <realthunder.dev@gmail.com> *
 *                                                                          *
 *   This file is part of the FreeCAD CAx development system.               *
 *                                                                          *
 *   This library is free software; you can redistribute it and/or          *
 *   modify it under the terms of the GNU Library General Public            *
 *   License as published by the Free Software Foundation; either           *
 *   version 2 of the License, or (at your option) any later version.       *
 *                                                                          *
 *   This library  is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 *   GNU Library General Public License for more details.                   *
 *                                                                          *
 *   You should have received a copy of the GNU Library General Public      *
 *   License along with this library; see the file COPYING.LIB. If not,     *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,          *
 *   Suite 330, Boston, MA  02111-1307, USA                                 *
 *                                                                          *
 ****************************************************************************/


#ifndef EXPRESSION_PARSER_H
#define EXPRESSION_PARSER_H

#include <tuple>
#include <Base/Interpreter.h>
#include "Expression.h"

namespace App {

////////////////////////////////////////////////////////////////////////////////////
// Expecting the extended expression is going to be constantly amended (to
// conform to Python), we move most of the class declarations here to avoid
// constant recompiling of the whole FC code base, as the expression header is
// included by everyone
///////////////////////////////////////////////////////////////////////////////////

_ExpressionAllocDefine(_ExpressionFastAlloc,boost::fast_pool_allocator);
#define ExpressionFastAlloc(_t) _ExpressionFastAlloc<_t> 

AppExport ExpressionPtr expressionFromPy(const App::DocumentObject *owner, const Py::Object &value);
AppExport Py::Object pyFromQuantity(const Base::Quantity &quantity);
AppExport bool pyToQuantity(Base::Quantity &q, const Py::Object &pyobj);
AppExport bool isPyMapping(const Py::Object &pyobj);

/// Check if the expression is either unit (number) expression or operator expression containing only unit
AppExport bool isSimpleExpression(const Expression *expr, bool no_arithmetics = false);

////////////////////////////////////////////////////////////////////////////////////

struct ExpressionNode;
typedef std::unique_ptr<ExpressionNode> ExpressionNodePtr;
struct ExpressionNode {
    const Expression *expr;
    int start;
    int end;
    std::vector<ExpressionNodePtr, ExpressionAllocator(ExpressionNodePtr)> children;
    std::map<App::ObjectIdentifier, bool> deps;

    void operator delete(void *p);
    static void* operator new(std::size_t sz);
};

AppExport ExpressionNodePtr getExpressionAST(std::ostringstream &ss, const Expression *expr);

/////////////////////////////////////////////////////////////////////////////////////////////

struct AppExport Expression::Component {
    ObjectIdentifier::Component comp;
    ExpressionPtr e1;
    ExpressionPtr e2;
    ExpressionPtr e3;

    Component(const std::string &n);
    Component(ExpressionPtr &&e1, ExpressionPtr &&e2, ExpressionPtr &&e3, bool isRange=false);
    Component(const ObjectIdentifier::Component &comp);
    Component(const Component &other);
    ~Component();
    Component &operator=(const Component &)=delete;

    static void* operator new(std::size_t sz);
    void operator delete(void *p);

    void visit(ExpressionVisitor &v);
    bool isTouched() const;
    void toString(std::ostream &ss, bool persistent) const;
    ComponentPtr copy() const;
    ComponentPtr eval() const;

    Py::Object get(const Expression *owner, const Py::Object &pyobj) const;
    void set(const Expression *owner, Py::Object &pyobj, const Py::Object &value) const;
    void del(const Expression *owner, Py::Object &pyobj) const;
};

////////////////////////////////////////////////////////////////////////////////////

struct AppExport VarInfo {
    Py::Object prefix;
    Py::Object *lhs;
    Py::Object rhs;
    Expression::ComponentPtr component;

    VarInfo()
        :lhs(0)
    {}

    VarInfo(Py::Object &v)
        :lhs(&v),rhs(v)
    {}

    VarInfo(VarInfo &&other)
        :prefix(other.prefix)
        ,lhs(other.lhs)
        ,rhs(other.rhs)
        ,component(std::move(other.component))
    {}

    VarInfo &operator=(VarInfo &&other) {
        lhs = other.lhs;
        prefix = other.prefix;
        rhs = other.rhs;
        component = std::move(other.component);
        return *this;
    }
};

/////////////////////////////////////////////////////////////////////////////////////

/// Helper class to disable calling into Python functions and writing Python attributes
class AppExport ExpressionFunctionCallDisabler
{
public:
    ExpressionFunctionCallDisabler(bool active = true);
    ~ExpressionFunctionCallDisabler();

    void setActive(bool active = true);
    bool isActive() const { return active; }
    static bool isFunctionCallDisabled();

private:
    /// Private new operator to prevent heap allocation
    void* operator new(size_t size);

private:
    bool active;

};

/// Exception thrown when invoking callable while it is disabled
class AppExport ExpressionFunctionDisabledException : Base::ExpressionError
{
public:
    using Base::ExpressionError::ExpressionError;
};

/// Helper class to block access of from expression
class AppExport ExpressionBlocker
{
public:
    ExpressionBlocker(const Expression *expr);
    ~ExpressionBlocker();
    /// Throw exception if expression is blocked
    static void check();

private:
    /// Private new operator to prevent heap allocation
    void* operator new(size_t size);
};

/////////////////////////////////////////////////////////////////////////////////////

#define EXPR_TYPESYSTEM_HEADER() \
    TYPESYSTEM_HEADER();\
public:\
    void operator delete(void *p)
        
/**
  * Part of an expressions that contains a unit.
  *
  */

class  AppExport UnitExpression : public Expression {
    EXPR_TYPESYSTEM_HEADER();
public:
    ~UnitExpression();

    static ExpressionPtr create(const App::DocumentObject *owner, const char *unitStr);
    static ExpressionPtr create(const App::DocumentObject *owner, const Base::Quantity &q);

    virtual ExpressionPtr simplify() const;

    void setQuantity(const Base::Quantity &_quantity);

    double getValue() const { return quantity.getValue(); }

    const Base::Unit & getUnit() const { return quantity.getUnit(); }

    const Base::Quantity & getQuantity() const { return quantity; }

    const char *getUnitString() const { return unitStr; }

    double getScaler() const { return quantity.getValue(); }

protected:
    UnitExpression(const App::DocumentObject *_owner, const Base::Quantity & _quantity, const char *unit=nullptr);

    virtual void _toString(std::ostream &ss, bool persistent, int indent) const;
    virtual ExpressionPtr _copy() const;
    virtual Py::Object _getPyValue(int *jumpCode=0) const;

protected:
    mutable PyObject *cache = 0;

private:
    Base::Quantity quantity;
    const char *unitStr; /**< The unit string from the original parsed string */
};

/**
  * Class implementing a number with an optional unit
  */

class AppExport NumberExpression : public UnitExpression {
    EXPR_TYPESYSTEM_HEADER();
public:
    static ExpressionPtr create(const App::DocumentObject *_owner, const Base::Quantity & quantity);
    static ExpressionPtr create(const App::DocumentObject *_owner, double fvalue);

    void negate();

    bool isInteger(long *v=0) const;

protected:
    NumberExpression(const App::DocumentObject *_owner, const Base::Quantity &q)
        :UnitExpression(_owner,q) 
    {}

    virtual void _toString(std::ostream &ss, bool persistent, int indent) const;
    virtual ExpressionPtr _copy() const;
};

class AppExport ConstantExpression : public NumberExpression {
    EXPR_TYPESYSTEM_HEADER();
public:
    static ExpressionPtr create(const App::DocumentObject *owner, 
            const char *name, const Base::Quantity &_quantity);

    const char *getName() const { return name; }

    bool isNumber() const;
    bool isBoolean(bool *value = nullptr) const;

    virtual ExpressionPtr simplify() const;

protected:
    ConstantExpression(const App::DocumentObject *_owner, const Base::Quantity &q)
        :NumberExpression(_owner,q)
    {}

    virtual Py::Object _getPyValue(int *jumpCode=0) const;
    virtual void _toString(std::ostream &ss, bool persistent, int indent) const;
    virtual ExpressionPtr _copy() const;

    const char *name; /**< Constant's name */
};

/**
  * Class implementing a reference to a property. If the name is unqualified,
  * the owner of the expression is searched. If it is qualified, the document
  * that contains the owning document object is searched for other document
  * objects to search. Both labels and internal document names are searched.
  *
  */
class AppExport VariableExpression : public Expression {
    EXPR_TYPESYSTEM_HEADER();
public:
    static ExpressionPtr create(const App::DocumentObject *owner, ObjectIdentifier &&var);

    virtual bool isTouched() const;

    std::string name() const;

    const ObjectIdentifier &getPath() const;

    virtual void addComponent(ComponentPtr &&component);

    std::vector<std::string> getStringList() const;

    // strip out given number of component
    void popComponents(int count=1);

    VarInfo push(const Expression *owner, bool mustExist, std::string *name=0) const;

    virtual ExpressionPtr simplify() const;

    void assign(const ObjectIdentifier &path) const;
    void assign(Py::Object value) const;

    static const VariableExpression *isDoubleBinding(const Expression *expr);

protected:
    VariableExpression(const App::DocumentObject *_owner):Expression(_owner) {}

    void setVarInfo(VarInfo &info, bool mustExist, bool noassign=false) const;

    virtual bool _isIndexable() const;
    virtual void _toString(std::ostream &ss, bool persistent, int indent) const;
    virtual ExpressionPtr _copy() const;
    virtual void _getIdentifiers(std::map<App::ObjectIdentifier,bool> &) const;
    virtual bool _adjustLinks(const std::set<App::DocumentObject*> &, ExpressionVisitor &);
    virtual void _importSubNames(const ObjectIdentifier::SubNameMap &);
    virtual void _updateLabelReference(App::DocumentObject *, const std::string &, const char *);
    virtual bool _updateElementReference(App::DocumentObject *,bool,ExpressionVisitor &);
    virtual bool _relabeledDocument(const std::string &, const std::string &, ExpressionVisitor &);
    virtual bool _renameObjectIdentifier(const std::map<ObjectIdentifier,ObjectIdentifier> &, 
                                         const ObjectIdentifier &, ExpressionVisitor &);
    virtual void _collectReplacement(std::map<ObjectIdentifier,ObjectIdentifier> &, 
                    const App::DocumentObject *parent, App::DocumentObject *oldObj, 
                    App::DocumentObject *newObj) const;
    virtual void _moveCells(const CellAddress &, int, int, ExpressionVisitor &);
    virtual void _offsetCells(int, int, ExpressionVisitor &);
    virtual Py::Object _getPyValue(int *jumpCode=0) const;

protected:
    ObjectIdentifier var; /**< Variable name  */
};

struct ExpressionString {
    std::string text;
    char prefix0;
    char prefix1;
    char quote;

    enum QuoteType{
        QuoteNormal,
        QuoteSingle,
        QuoteDouble,
        QuoteSingleLong,
        QuoteDoubleLong,
    };

    ExpressionString(std::string &&txt = std::string())
        :text(std::move(txt))
        ,prefix0(0)
        ,prefix1(0)
        ,quote(0)
    {
    }

    ExpressionString(const ExpressionString &other)
        :text(other.text),prefix0(other.prefix0),prefix1(other.prefix1),quote(other.quote)
    {
    }
    ExpressionString(ExpressionString &&other)
        :text(std::move(other.text)),prefix0(other.prefix0),prefix1(other.prefix1),quote(other.quote)
    {
    }
    ExpressionString &operator=(const ExpressionString &other) {
        text = other.text;
        prefix0 = other.prefix0;
        prefix1 = other.prefix1;
        quote = other.quote;
        return *this;
    }
    ExpressionString &operator=(ExpressionString &&other) {
        text = std::move(other.text);
        prefix0 = other.prefix0;
        prefix1 = other.prefix1;
        quote = other.quote;
        return *this;
    }
    bool isRaw() const {
        return prefix0=='r'||prefix1=='r';
    }
    bool isFormatted() const {
        return prefix0=='f'||prefix1=='f';
    }
    bool isLong() const {
        return quote==QuoteSingleLong || quote==QuoteDoubleLong;
    }
    static ExpressionString unquote(const char *txt);
};

/**
  * Class implementing a string. Used to signal either a genuine string or
  * a failed evaluation of an expression.
  */
class AppExport StringExpression : public Expression {
    EXPR_TYPESYSTEM_HEADER();
public:
    ~StringExpression();

    static ExpressionPtr create(const App::DocumentObject *owner, ExpressionString &&text);

    static ExpressionPtr create(const App::DocumentObject *owner, const char *txt) {
        return create(owner,ExpressionString(std::string(txt?txt:"")));
    }

    virtual const std::string &getText() const { return str.text; }

    void append(const ExpressionString &);

protected:
    StringExpression(const App::DocumentObject *_owner):Expression(_owner) {}

    virtual bool _isIndexable() const { return true; }
    virtual ExpressionPtr _copy() const;
    virtual void _toString(std::ostream &ss, bool persistent, int indent) const;
    virtual Py::Object _getPyValue(int *jumpCode=0) const;

private:
    ExpressionString str;
    mutable PyObject *cache = 0;
};

//////////////////////////////////////////////////////////////////////

class AppExport PyObjectExpression : public Expression {
    EXPR_TYPESYSTEM_HEADER();

public:
    static ExpressionPtr create(const App::DocumentObject *owner, PyObject *pyobj=0);
    virtual ~PyObjectExpression();

    void setPyObject(Py::Object pyobj);
    void setPyObject(PyObject *pyobj, bool owned=false);

protected:
    PyObjectExpression(const App::DocumentObject *_owner):Expression(_owner) {}

    virtual Py::Object _getPyValue(int *jumpCode=0) const;
    virtual void _toString(std::ostream &,bool, int) const;
    virtual ExpressionPtr _copy() const;

protected:
    PyObject *pyObj = 0;
};

/**
  * Class implementing an infix expression.
  *
  */

class AppExport OperatorExpression : public Expression {
    EXPR_TYPESYSTEM_HEADER();
public:
    static ExpressionPtr create(const App::DocumentObject *owner, 
            ExpressionPtr &&left, int op, ExpressionPtr &&right);

    virtual bool isTouched() const;

    virtual ExpressionPtr simplify() const;

    virtual int priority() const;

    int getOperator() const { return op; }

    const Expression * getLeft() const { return left.get(); }

    const Expression * getRight() const { return right.get(); }

protected:
    OperatorExpression(const App::DocumentObject *_owner):Expression(_owner){}

    virtual void _visit(ExpressionVisitor & v);
    virtual void _toString(std::ostream &ss, bool persistent, int indent) const;
    virtual ExpressionPtr _copy() const;
    virtual Py::Object _getPyValue(int *jumpCode=0) const;

    virtual bool isCommutative() const;

    virtual bool isLeftAssociative() const;

    virtual bool isRightAssociative() const;

    int op=0;        /**< Operator working on left and right */
    ExpressionPtr left;  /**< Left operand */
    ExpressionPtr right; /**< Right operand */
};

class AppExport AssignmentExpression : public Expression {
    EXPR_TYPESYSTEM_HEADER();

public:
    static ExpressionPtr create(const App::DocumentObject *_owner, 
            int catchAll, ExpressionList &&left, ExpressionList &&right, int op=0);

    static ExpressionPtr create(const App::DocumentObject *_owner, 
            ExpressionPtr &&left, ExpressionPtr &&right, int op=0);

    virtual bool isTouched() const;

    static Py::Object apply(const Expression *owner, int catchAll, 
            const ExpressionList &left, const Expression *right, int op=0);

protected:
    AssignmentExpression(const App::DocumentObject *_owner) :Expression(_owner) {}

    virtual void _visit(ExpressionVisitor & v);
    virtual void _toString(std::ostream &ss, bool persistent, int indent) const;
    virtual ExpressionPtr _copy() const;
    virtual Py::Object _getPyValue(int *jumpCode=0) const;

    static void assign(const Expression *owner, const Expression *left, PyObject *right);

protected:
    ExpressionList left;
    ExpressionPtr right;
    int catchAll = -1;
    int op = 0;
    bool rightTuple = false;
};

class AppExport ConditionalExpression : public Expression {
    EXPR_TYPESYSTEM_HEADER();
public:
    static ExpressionPtr create(const App::DocumentObject *owner, ExpressionPtr &&condition, 
            ExpressionPtr &&trueExpr,  ExpressionPtr &&falseExpr, bool python_form=false);

    virtual bool isTouched() const;

    virtual ExpressionPtr simplify() const;

    virtual int priority() const;

protected:
    ConditionalExpression(const App::DocumentObject *_owner)
        :Expression(_owner)
    {}

    virtual void _visit(ExpressionVisitor & v);
    virtual void _toString(std::ostream &ss, bool persistent, int indent) const;
    virtual ExpressionPtr _copy() const;
    virtual Py::Object _getPyValue(int *jumpCode=0) const;

    ExpressionPtr condition;  /**< Condition */
    ExpressionPtr trueExpr;  /**< Expression if abs(condition) is > 0.5 */
    ExpressionPtr falseExpr; /**< Expression if abs(condition) is < 0.5 */
    bool pythonForm = false;
};

/**
  * Class implementing various functions, e.g sin, cos, etc.
  *
  */

class AppExport FunctionExpression : public Expression {
    EXPR_TYPESYSTEM_HEADER();
public:
    static ExpressionPtr create(const App::DocumentObject *owner, int f, ExpressionList &&args);

    virtual bool isTouched() const;

    virtual ExpressionPtr simplify() const;

    enum FunctionType {
        FUNC_NONE,

        // Normal functions taking one or two arguments
        ACOS,
        ASIN,
        ATAN,
        ABS,
        EXP,
        LOG,
        LOG10,
        SIN,
        SINH,
        TAN,
        TANH,
        SQRT,
        COS,
        COSH,
        ATAN2,
        FMOD,
        FPOW,
        ROUND,
        TRUNC,
        CEIL,
        FLOOR,
        HYPOT,
        CATH,

        GET_VAR,
        HAS_VAR,
        IMPORT_PY,
        PRAGMA,
        LIST,
        TUPLE,
        MSCALE, // matrix scale by vector
        MINVERT, // invert matrix/placement/rotation
        CREATE, // create new object of a given type
        STR, // stringify
        HREF,
        HIDDEN_REF,

        // double binding, used by PropertyExpressionEngine to make a property both driving and driven
        DBIND, 

        CALLABLE_START,

        FUNC,
        FUNC_D,
        FUNC_PARSED,
        EVAL,

        CALLABLE_END,

        // Aggregates
        AGGREGATES,

        SUM,
        AVERAGE,
        STDDEV,
        COUNT,
        MIN,
        MAX,

        // Last one
        LAST,
    };
    int type() const {return ftype;}

    static Py::Object evaluate(const Expression *owner, int type, const ExpressionList &args);

    const ExpressionList &getArgs() const {return args;}

    struct FunctionInfo {
        FunctionType type;
        const char *name;
        const char *description;
    };
    static const std::vector<FunctionInfo> &getFunctions();

protected:
    FunctionExpression(const App::DocumentObject *_owner):Expression(_owner){}

    virtual void _visit(ExpressionVisitor & v);
    virtual void _toString(std::ostream &ss, bool persistent, int indent) const;
    virtual ExpressionPtr _copy() const;
    virtual Py::Object _getPyValue(int *jumpCode=0) const;
    static Py::Object evalAggregate(const Expression *owner, int type, const ExpressionList &args);

    int ftype;        /**< Function to execute */
    ExpressionList args; /** Arguments to function*/
};

/**
  * Class implementing a callable expression with named arguments and optional trailing accessor
  */

class AppExport CallableExpression : public FunctionExpression {
    EXPR_TYPESYSTEM_HEADER();
public:
    static ExpressionPtr create(const App::DocumentObject *owner, std::string &&name,
            StringList &&names=StringList(), ExpressionList &&args=ExpressionList());

    static ExpressionPtr create(const App::DocumentObject *owner, ObjectIdentifier &&path,
            StringList &&names=StringList(), ExpressionList &&args=ExpressionList());

    static ExpressionPtr create(const DocumentObject *owner, ExpressionPtr &&expr, 
            StringList &&names=StringList(), ExpressionList &&args=ExpressionList(), 
            int ftype=0, std::string &&name = std::string(), bool checkArgs=true);

    Py::Object evaluate(PyObject *tuple, PyObject *kwds);

    VarInfo getVarInfo(bool mustExist) const;

    virtual bool isTouched() const;

    std::string getDocString() const;
    const std::string &getName() const {return name;}

    static void securityCheck(PyObject *pyobj, PyObject *attr);

protected:
    CallableExpression(const App::DocumentObject *_owner):FunctionExpression(_owner) {}

    virtual void _visit(ExpressionVisitor & v);
    virtual bool _isIndexable() const { return true; }
    virtual Py::Object _getPyValue(int *jumpCode=0) const;
    virtual void _toString(std::ostream &ss, bool persistent, int indent) const;
    virtual ExpressionPtr _copy() const;

    static void securityCheck(PyObject *pyobj=nullptr, const Expression *expr=nullptr);

protected:
    ExpressionPtr expr;
    std::string name;
    StringList names;
};

class AppExport RangeExpression : public App::Expression {
    EXPR_TYPESYSTEM_HEADER();
public:
    static ExpressionPtr create(const App::DocumentObject * owner,
            std::string &&begin, std::string &&end);

    virtual bool isTouched() const;

    Range getRange() const;

protected:
    RangeExpression(const App::DocumentObject *_owner):Expression(_owner){}

    virtual void _toString(std::ostream &, bool, int) const;
    virtual ExpressionPtr _copy() const;
    virtual void _getIdentifiers(std::map<App::ObjectIdentifier,bool> &) const;
    virtual bool _renameObjectIdentifier(const std::map<ObjectIdentifier,ObjectIdentifier> &, 
                                         const ObjectIdentifier &, ExpressionVisitor &);
    virtual void _moveCells(const CellAddress &, int, int, ExpressionVisitor &);
    virtual void _offsetCells(int, int, ExpressionVisitor &);
    virtual Py::Object _getPyValue(int *jumpCode=0) const;

protected:
    std::string begin;
    std::string end;
};

//////////////////////////////////////////////////////////////////////

class AppExport ComprehensionExpression : public Expression {
    EXPR_TYPESYSTEM_HEADER();

public:
    static ExpressionPtr create(const App::DocumentObject * owner, 
            int catchAll, ExpressionList &&targets, ExpressionPtr &&expr);

    virtual bool isTouched() const;

    void setExpr(ExpressionPtr &&key, ExpressionPtr &&value);
    void setExpr(ExpressionPtr &&key, bool isList=true);

    void add(int catchAll, ExpressionList &&targets, ExpressionPtr &&expr);
    void addCond(ExpressionPtr &&cond);

protected:
    ComprehensionExpression(const App::DocumentObject *_owner):Expression(_owner) {}

    virtual void _visit(ExpressionVisitor & v);
    virtual bool _isIndexable() const {return true;}
    virtual Py::Object _getPyValue(int *jumpCode=0) const;
    virtual void _toString(std::ostream &, bool, int) const;
    virtual ExpressionPtr _copy() const;

    struct CompFor {
        ExpressionList targets;
        ExpressionPtr expr;
        ExpressionList conds;
        int catchAll;

        CompFor():catchAll(-1) 
        {}
        CompFor(CompFor &&other)
            :targets(std::move(other.targets))
            ,expr(std::move(other.expr))
            ,conds(std::move(other.conds))
            ,catchAll(other.catchAll)
        {}
        CompFor(const CompFor &other);
        CompFor &operator=(const CompFor &other) = delete;
    };
    typedef std::list<CompFor, ExpressionFastAlloc(CompFor) > CompForList;
    void _calc(Py::Object &res, CompForList::const_iterator iter) const;

protected:
    ExpressionPtr key;
    ExpressionPtr value;
    CompForList comps;
    bool list = false;
};

//////////////////////////////////////////////////////////////////////

class AppExport ListExpression : public Expression {
    EXPR_TYPESYSTEM_HEADER();

public:
    typedef std::vector<bool>                       FlagList;
    typedef std::pair<ExpressionPtr, bool>          FlagExpression;
    typedef std::pair<ExpressionList, FlagList>     FlagExpressionList;

    static ExpressionPtr create(const App::DocumentObject *owner);

    static ExpressionPtr create(const App::DocumentObject *owner, 
            ExpressionList &&items, FlagList &&flags = FlagList());

    virtual bool isTouched() const;

    void printItems(std::ostream &ss, bool persistent) const;

    std::size_t getSize() const {
        return items.size();
    }

    void setItem(std::size_t index, ExpressionPtr &&expr, bool expand=false);
    void append(ExpressionPtr &&expr, bool expand=false);

    const ExpressionList &getItems() const {return items;}

protected:
    ListExpression(const App::DocumentObject *_owner):Expression(_owner) {}

    virtual void _visit(ExpressionVisitor & v);
    virtual bool _isIndexable() const {return true;}
    virtual Py::Object _getPyValue(int *jumpCode=0) const;
    virtual void _toString(std::ostream &, bool, int) const;
    virtual ExpressionPtr _copy() const;

    friend class AssignmentExpression;

protected:
    ExpressionList items;
    FlagList flags;
};

//////////////////////////////////////////////////////////////////////

class AppExport TupleExpression : public ListExpression {
    EXPR_TYPESYSTEM_HEADER();

public:
    static ExpressionPtr create(const App::DocumentObject *owner);

    static ExpressionPtr create(const App::DocumentObject *owner, ExpressionPtr &&expr, bool flag);

    static ExpressionPtr create(const App::DocumentObject *owner, 
            ExpressionList &&items, FlagList &&flags = FlagList());

protected:
    TupleExpression(const App::DocumentObject *_owner):ListExpression(_owner) {}

    virtual Py::Object _getPyValue(int *jumpCode=0) const;
    virtual ExpressionPtr _copy() const;
    virtual void _toString(std::ostream &, bool, int) const;
};

//////////////////////////////////////////////////////////////////////

class AppExport DictExpression : public Expression {
    EXPR_TYPESYSTEM_HEADER();

public:
    static ExpressionPtr create(const App::DocumentObject * owner);

    static ExpressionPtr create(const App::DocumentObject *owner, 
            ExpressionPtr &&key, ExpressionPtr &&value);

    static ExpressionPtr create(const App::DocumentObject *owner, ExpressionPtr &&value);

    virtual bool isTouched() const;

    void addItem(ExpressionPtr &&key, ExpressionPtr &&value);
    void addItem(ExpressionPtr &&value);

protected:
    DictExpression(const App::DocumentObject *_owner):Expression(_owner) {}

    virtual void _visit(ExpressionVisitor & v);
    virtual bool _isIndexable() const {return true;}
    virtual Py::Object _getPyValue(int *jumpCode=0) const;
    virtual void _toString(std::ostream &, bool, int) const;
    virtual ExpressionPtr _copy() const;

protected:
    ExpressionList keys;
    ExpressionList values;
};

//////////////////////////////////////////////////////////////////////

class AppExport IDictExpression : public Expression {
    EXPR_TYPESYSTEM_HEADER();

public:
    static ExpressionPtr create(const App::DocumentObject * owner);

    static ExpressionPtr create(const App::DocumentObject * owner, 
            std::string &&key, ExpressionPtr &&value);

    static ExpressionPtr create(const App::DocumentObject * owner, 
            const char *key, ExpressionPtr &&value);

    virtual bool isTouched() const;

    void addItem(std::string &&key, ExpressionPtr &&value);
    void addItem(const char *key, ExpressionPtr &&value);

    std::map<std::string, ExpressionPtr> getItems() const;
    
protected:
    IDictExpression(const App::DocumentObject *_owner):Expression(_owner) {}

    virtual void _visit(ExpressionVisitor & v);
    virtual bool _isIndexable() const {return true;}
    virtual Py::Object _getPyValue(int *jumpCode=0) const;
    virtual void _toString(std::ostream &, bool, int) const;
    virtual ExpressionPtr _copy() const;

protected:
    StringList keys;
    ExpressionList values;
};

/////////////////////////////////////////////////////////////////

class AppExport BaseStatement : public Expression {
    EXPR_TYPESYSTEM_HEADER();
public:
protected:
    BaseStatement(const App::DocumentObject *owner):Expression(owner){}
};

/////////////////////////////////////////////////////////////////

class AppExport PseudoStatement : public BaseStatement {
    EXPR_TYPESYSTEM_HEADER();

public:
    static ExpressionPtr create(const App::DocumentObject *owner, int type);

protected:
    PseudoStatement(const App::DocumentObject *_owner,int _t)
        :BaseStatement(_owner),type(_t)
    {}

    virtual void _toString(std::ostream &ss, bool persistent, int indent) const;
    virtual ExpressionPtr _copy() const;
    virtual Py::Object _getPyValue(int *jumpCode=0) const;

protected:
    int type;
};

/////////////////////////////////////////////////////////////////

class AppExport JumpStatement : public BaseStatement {
    EXPR_TYPESYSTEM_HEADER();

public:
    static ExpressionPtr create(const App::DocumentObject *owner, 
                    int type, ExpressionPtr &&expr = ExpressionPtr());

    virtual bool isTouched() const;

protected:
    JumpStatement(const App::DocumentObject *_owner) :BaseStatement(_owner) {}

    virtual void _visit(ExpressionVisitor & v);
    virtual void _toString(std::ostream &ss, bool persistent, int indent) const;
    virtual ExpressionPtr _copy() const;
    virtual Py::Object _getPyValue(int *jumpCode=0) const;

protected:
    ExpressionPtr expr;
    int type = 0;
};

/////////////////////////////////////////////////////////////////

class AppExport IfStatement : public BaseStatement {
    EXPR_TYPESYSTEM_HEADER();

public:
    static ExpressionPtr create(const App::DocumentObject *owner, 
            ExpressionPtr &&condition, ExpressionPtr &&statement);

    void add(ExpressionPtr &&condition, ExpressionPtr &&statement);
    void addElse(ExpressionPtr &&statement);

    virtual bool isTouched() const;
    virtual bool needLineEnd() const;

protected:
    IfStatement(const App::DocumentObject *_owner):BaseStatement(_owner) {}

    virtual void _visit(ExpressionVisitor & v);
    virtual void _toString(std::ostream &ss, bool persistent, int indent) const;
    virtual ExpressionPtr _copy() const;
    virtual Py::Object _getPyValue(int *jumpCode=0) const;

protected:
    ExpressionList conditions;
    ExpressionList statements;
};

/////////////////////////////////////////////////////////////////

class AppExport WhileStatement : public BaseStatement {
    EXPR_TYPESYSTEM_HEADER();

public:
    static ExpressionPtr create(const App::DocumentObject *owner, 
            ExpressionPtr &&condition, ExpressionPtr &&statement);

    void addElse(ExpressionPtr &&expr);

    virtual bool isTouched() const;
    virtual bool needLineEnd() const;

protected:
    WhileStatement(const App::DocumentObject *_owner):BaseStatement(_owner) {}

    virtual void _visit(ExpressionVisitor & v);
    virtual void _toString(std::ostream &ss, bool persistent, int indent) const;
    virtual ExpressionPtr _copy() const;
    virtual Py::Object _getPyValue(int *jumpCode=0) const;

protected:
    ExpressionPtr condition;
    ExpressionPtr statement;
    ExpressionPtr else_expr;
};

/////////////////////////////////////////////////////////////////

class AppExport ForStatement : public BaseStatement {
    EXPR_TYPESYSTEM_HEADER();

public:
    static ExpressionPtr create(const App::DocumentObject *owner, 
            int catchAll, ExpressionList &&targets, ExpressionList &&exprs, ExpressionPtr &&statement);

    void addElse(ExpressionPtr &&expr);

    virtual bool isTouched() const;
    virtual bool needLineEnd() const;

protected:
    ForStatement(const App::DocumentObject *_owner):BaseStatement(_owner) {}

    virtual void _visit(ExpressionVisitor & v);
    virtual void _toString(std::ostream &ss, bool persistent, int indent) const;
    virtual ExpressionPtr _copy() const;
    virtual Py::Object _getPyValue(int *jumpCode=0) const;

protected:
    ExpressionList targets;
    ExpressionPtr value;
    ExpressionPtr statement;
    ExpressionPtr else_expr;
    int catchAll = -1;
    bool valueTuple = false;
};

/////////////////////////////////////////////////////////////////

class AppExport SimpleStatement : public BaseStatement {
    EXPR_TYPESYSTEM_HEADER();

public:
    static ExpressionPtr create(const App::DocumentObject *owner, ExpressionPtr &&expr);

    virtual bool isTouched() const;
    void add(ExpressionPtr &&expr);

    std::size_t getSize() const {return exprs.size();}
    const Expression *getExpr(std::size_t idx) const;

    template<typename T>
    static const T *cast(const Expression *expr) {
        auto res = Base::freecad_dynamic_cast<const T>(expr);
        if(res)
            return res;
        auto stmt = Base::freecad_dynamic_cast<const SimpleStatement>(expr);
        if(stmt && stmt->getSize()==1)
            return cast<T>(stmt->getExpr(0));
        return 0;
    }

    ExpressionPtr reduce() const;

protected:
    SimpleStatement(const App::DocumentObject *_owner):BaseStatement(_owner) {}

    virtual void _visit(ExpressionVisitor & v);
    virtual void _toString(std::ostream &ss, bool persistent, int indent) const;
    virtual ExpressionPtr _copy() const;
    virtual Py::Object _getPyValue(int *jumpCode=0) const;

protected:
    ExpressionList exprs;
};

/////////////////////////////////////////////////////////////

class AppExport Statement : public SimpleStatement {
    EXPR_TYPESYSTEM_HEADER();

public:
    static ExpressionPtr create(const App::DocumentObject *owner, ExpressionPtr &&expr);

protected:
    Statement(const App::DocumentObject *_owner):SimpleStatement(_owner) {}

    virtual void _toString(std::ostream &ss, bool persistent, int indent) const;
    virtual ExpressionPtr _copy() const;
};

/////////////////////////////////////////////////////////////

class AppExport LambdaExpression : public Expression {
    EXPR_TYPESYSTEM_HEADER();

public:
    static ExpressionPtr create(const App::DocumentObject *owner, ExpressionPtr &&body, 
            StringList &&names=StringList(), ExpressionList &&args=ExpressionList());

    virtual bool isTouched() const;

protected:
    LambdaExpression(const App::DocumentObject *_owner):Expression(_owner) {}

    virtual void _visit(ExpressionVisitor & v);
    virtual void _toString(std::ostream &ss, bool persistent, int indent) const;
    virtual ExpressionPtr _copy() const;
    virtual Py::Object _getPyValue(int *jumpCode=0) const;

protected:
    StringList names;
    ExpressionList args;
    ExpressionPtr body;
};

/////////////////////////////////////////////////////////////

class AppExport FunctionStatement : public LambdaExpression {
    EXPR_TYPESYSTEM_HEADER();

public:
    static ExpressionPtr create(const App::DocumentObject *owner, std::string &&name, 
            ExpressionPtr &&body, StringList &&names=StringList(), ExpressionList &&args=ExpressionList());

    virtual bool needLineEnd() const;

    const std::string &getName() const {return name;}

protected:
    FunctionStatement(const App::DocumentObject *_owner):LambdaExpression(_owner) {}

    virtual void _toString(std::ostream &ss, bool persistent, int indent) const;
    virtual ExpressionPtr _copy() const;
    virtual Py::Object _getPyValue(int *jumpCode=0) const;

protected:
    std::string name;
};


/////////////////////////////////////////////////////////////

class AppExport DelStatement : public BaseStatement {
    EXPR_TYPESYSTEM_HEADER();

public:
    static ExpressionPtr create(const App::DocumentObject *owner, ExpressionList &&targets);

    virtual bool isTouched() const;

protected:
    DelStatement(const App::DocumentObject *_owner):BaseStatement(_owner) {}

    virtual void _visit(ExpressionVisitor & v);
    virtual void _toString(std::ostream &ss, bool persistent, int indent) const;
    virtual ExpressionPtr _copy() const;
    virtual Py::Object _getPyValue(int *jumpCode=0) const;

protected:
    ExpressionList targets;
};

/////////////////////////////////////////////////////////////

class AppExport ScopeStatement : public BaseStatement {
    EXPR_TYPESYSTEM_HEADER();

public:
    static ExpressionPtr create(const App::DocumentObject *owner, StringList &&names, bool global=true); 

protected:
    ScopeStatement(const App::DocumentObject *_owner):BaseStatement(_owner) {}

    virtual void _toString(std::ostream &ss, bool persistent, int indent) const;
    virtual ExpressionPtr _copy() const;
    virtual Py::Object _getPyValue(int *jumpCode=0) const;

protected:
    StringList names;
    bool global = true;
};

/////////////////////////////////////////////////////////////

class AppExport TryStatement : public BaseStatement {
    EXPR_TYPESYSTEM_HEADER();

public:
    static ExpressionPtr create(const App::DocumentObject *owner, ExpressionPtr &&body);

    void add(ExpressionPtr &&body, ExpressionPtr &&expr=ExpressionPtr(), std::string &&name=std::string());
    void addElse(ExpressionPtr &&body);
    void addFinal(ExpressionPtr &&body);
    void check();
    virtual bool needLineEnd() const;

protected:
    TryStatement(const App::DocumentObject *_owner):BaseStatement(_owner) {}

    virtual void _toString(std::ostream &ss, bool persistent, int indent) const;
    virtual ExpressionPtr _copy() const;
    virtual Py::Object _getPyValue(int *jumpCode=0) const;

    bool findException(Py::Object &res, int *jumpCode, Base::Exception &e, PyObject *pyobj) const;

protected:
    ExpressionPtr body;
    StringList names;
    ExpressionList exprs;
    ExpressionList bodies;
    ExpressionPtr else_body;
    ExpressionPtr final_body;
};

/////////////////////////////////////////////////////////////

class AppExport ImportStatement : public BaseStatement {
    EXPR_TYPESYSTEM_HEADER();

public:
    static ExpressionPtr create(const App::DocumentObject *owner, std::string &&module,
            std::string &&name = std::string());

    void add(std::string &&module, std::string &&name = std::string());

protected:
    ImportStatement(const App::DocumentObject *_owner):BaseStatement(_owner) {}

    virtual void _toString(std::ostream &ss, bool persistent, int indent) const;
    virtual ExpressionPtr _copy() const;
    virtual Py::Object _getPyValue(int *jumpCode=0) const;

protected:
    StringList modules;
    StringList names;
};

/////////////////////////////////////////////////////////////////////////////////////

class AppExport FromStatement : public BaseStatement {
    EXPR_TYPESYSTEM_HEADER();

public:
    static ExpressionPtr create(const App::DocumentObject *owner, std::string &&module,
            std::string &&tail, std::string &&name = std::string());

    void add(std::string &&tail, std::string &&name = std::string());

protected:
    FromStatement(const App::DocumentObject *_owner):BaseStatement(_owner) {}

    virtual void _toString(std::ostream &ss, bool persistent, int indent) const;
    virtual ExpressionPtr _copy() const;
    virtual Py::Object _getPyValue(int *jumpCode=0) const;

protected:
    std::string module;
    StringList tails;
    StringList names;
};

/////////////////////////////////////////////////////////////////////////////////////

namespace ExpressionParser {

AppExport ExpressionPtr parse(const App::DocumentObject *owner, 
        const char *buffer, std::size_t len=0, bool verbose=false, bool pythonMode=false);
AppExport ExpressionPtr parseUnit(const App::DocumentObject *owner, const char *buffer);
AppExport ObjectIdentifier parsePath(const App::DocumentObject *owner, const char* buffer, std::size_t len=0);
AppExport bool isTokenAnIndentifier(const std::string & str);
AppExport bool isTokenAUnit(const std::string & str);

AppExport std::vector<std::tuple<int, int, std::string> > tokenize(const char *str);

inline std::vector<std::tuple<int, int, std::string> > tokenize(const std::string & str) {
    return tokenize(str.c_str());
}

AppExport void clearWarning();

enum TokenType {
    FC_TOK_END,
    FC_TOK_LITERAL,
    FC_TOK_KEYWORD,
    FC_TOK_IDENTIFIER,
    FC_TOK_STRING,
    FC_TOK_UNIT,
    FC_TOK_CELLADDRESS,
    FC_TOK_NUMBER,
    FC_TOK_OPERATOR,
    FC_TOK_OTHER,
};
AppExport int translateToken(int t);

/// Convenient class to mark begin of importing
class AppExport ExpressionImporter {
public:
    ExpressionImporter(Base::XMLReader &reader);
    ~ExpressionImporter();
    static Base::XMLReader *reader();
};

AppExport bool isModuleImported(PyObject *);

} // end of namespace ExpressionParser
} // end of namespace App

#endif //EXPRESSION_PARSER_H

