/***************************************************************************
 *   Copyright (c) 2008 Werner Mayer <wmayer[at]users.sourceforge.net>     *
 *                                                                         *
 *   This file is part of the FreeCAD CAx development system.              *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Library General Public           *
 *   License as published by the Free Software Foundation; either          *
 *   version 2 of the License, or (at your option) any later version.      *
 *                                                                         *
 *   This library  is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this library; see the file COPYING.LIB. If not,    *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,         *
 *   Suite 330, Boston, MA  02111-1307, USA                                *
 *                                                                         *
 ***************************************************************************/


#include "PreCompiled.h"
#ifndef _PreComp_
# include <BRepAlgo.hxx>
# include <BRepFilletAPI_MakeFillet.hxx>
# include <TopExp_Explorer.hxx>
# include <TopoDS.hxx>
# include <TopoDS_Edge.hxx>
# include <TopTools_ListOfShape.hxx>
# include <TopTools_IndexedMapOfShape.hxx>
# include <TopExp.hxx>
# include <BRep_Tool.hxx>
# include <ShapeFix_Shape.hxx>
# include <ShapeFix_ShapeTolerance.hxx>
#include <ShapeUpgrade_ShapeDivideClosed.hxx>
#include <ShapeBuild_ReShape.hxx>
#endif

#include <Base/Console.h>
#include <Base/Exception.h>
#include <Base/Reader.h>
#include <App/Document.h>
#include <Mod/Part/App/TopoShape.h>

#include "FeatureFillet.h"
#include "assert.h"


using namespace PartDesign;


PROPERTY_SOURCE(PartDesign::Fillet, PartDesign::DressUp)

const App::PropertyQuantityConstraint::Constraints floatRadius = {0.0,FLT_MAX,0.1};

Fillet::Fillet()
{
    ADD_PROPERTY(Radius,(1.0));
    Radius.setUnit(Base::Unit::Length);
    Radius.setConstraints(&floatRadius);
}

short Fillet::mustExecute() const
{
    if (Placement.isTouched() || Radius.isTouched())
        return 1;
    return DressUp::mustExecute();
}

App::DocumentObjectExecReturn *Fillet::execute(void)
{
    Part::TopoShape baseShape;
    try {
        baseShape = getBaseShape();
    } catch (Base::Exception& e) {
        return new App::DocumentObjectExecReturn(e.what());
    }
    baseShape.setTransform(Base::Matrix4D());

    auto edges = getContinuousEdges(baseShape);
    if (edges.size() == 0)
        return new App::DocumentObjectExecReturn("Fillet not possible on selected shapes");

    double radius = Radius.getValue();

    auto faces = getFaces(baseShape);
    TopoDS_Shape baseSh=baseShape.getShape();
    ShapeUpgrade_ShapeDivideClosed SDC(baseSh);
    SDC.Perform();
    if(!SDC.Result().IsEqual(baseSh)){
      std::vector<TopoShape> addedShapes;
      TopTools_MapOfShape addedEdges;
      TopoDS_Shape splitSh=SDC.Result();
      Handle(ShapeBuild_ReShape) SDC_context=SDC.GetContext();
      std::vector<TopoShape> _edges;
      for  (long i=0; i<edges.size(); i++){
	    TopoDS_Shape oldSh=edges[i].getShape();
	    TopoDS_Shape newSh=SDC_context->Apply(oldSh);
	    if(newSh.IsEqual(oldSh)) _edges.push_back(edges[i]);
	    else for (TopExp_Explorer exp(newSh,TopAbs_EDGE); exp.More(); exp.Next()){
	     TopoShape e(exp.Current()); 
	     _edges.push_back(e);
	     addedShapes.push_back(e);
             addedEdges.Add(exp.Current());
	    }
      }
      edges=_edges;
      std::vector<TopoShape> removedShapes;
      for (TopExp_Explorer exp(baseSh,TopAbs_EDGE); exp.More(); exp.Next()){
	  TopoDS_Shape oldSh=exp.Current();
	  TopoDS_Shape newSh=SDC_context->Apply(oldSh);
	  if(!newSh.IsEqual(oldSh)){
            for (TopExp_Explorer exp1(newSh,TopAbs_EDGE); exp1.More(); exp1.Next()){
	      TopoShape e(exp1.Current()); 
	      if(!addedEdges.Contains(exp1.Current())) addedShapes.push_back(TopoShape(exp1.Current()));
	    }
	    removedShapes.push_back(TopoShape(oldSh));
	  }
      }
      for (TopExp_Explorer exp(baseSh,TopAbs_WIRE); exp.More(); exp.Next()){
	  TopoDS_Shape oldSh=exp.Current();
	  TopoDS_Shape newSh=SDC_context->Apply(oldSh);
	  if(!newSh.IsEqual(oldSh)){
            for (TopExp_Explorer exp1(newSh,TopAbs_WIRE); exp1.More(); exp1.Next()) addedShapes.push_back(TopoShape(exp1.Current()));
	    removedShapes.push_back(TopoShape(oldSh));
	  }
      }
      for (TopExp_Explorer exp(baseSh,TopAbs_FACE); exp.More(); exp.Next()){
	  TopoDS_Shape oldSh=exp.Current();
	  TopoDS_Shape newSh=SDC_context->Apply(oldSh);
	  if(!newSh.IsEqual(oldSh)){
            for (TopExp_Explorer exp1(newSh,TopAbs_FACE); exp1.More(); exp1.Next()) addedShapes.push_back(TopoShape(exp1.Current()));
	    removedShapes.push_back(TopoShape(oldSh));
	  }
      }
      std::vector< std::pair<TopoShape,TopoShape> > replacedShapes;
      for (TopExp_Explorer exp(baseSh,TopAbs_SHELL); exp.More(); exp.Next()){
	  TopoDS_Shape oldSh=exp.Current();
	  TopoDS_Shape newSh=SDC_context->Apply(oldSh);
	  if(!newSh.IsEqual(oldSh)){
              replacedShapes.push_back(std::pair<TopoShape,TopoShape>(TopoShape(oldSh),TopoShape(newSh)));
	  }
      }
      for (TopExp_Explorer exp(baseSh,TopAbs_SOLID); exp.More(); exp.Next()){
	  TopoDS_Shape oldSh=exp.Current();
	  TopoDS_Shape newSh=SDC_context->Apply(oldSh);
	  if(!newSh.IsEqual(oldSh)){
              replacedShapes.push_back(std::pair<TopoShape,TopoShape>(TopoShape(oldSh),TopoShape(newSh)));
	  }
      }
      baseShape=baseShape.replacEShape(replacedShapes);
      baseShape=baseShape.removEShape(removedShapes);
      baseShape.mapSubElement(addedShapes,"FLT");
    }

    if(radius <= 0)
        return new App::DocumentObjectExecReturn("Fillet radius must be greater than zero");

    this->positionByBaseFeature();

    try {

       TopoDS_Shape theShape=baseShape.getShape();
       TopTools_MapOfShape allEdges;
       for (TopExp_Explorer exp(theShape,TopAbs_EDGE); exp.More(); exp.Next()) allEdges.Add(exp.Current());
       for  (long i=0; i<edges.size(); i++){
	    TopoDS_Shape edge=edges[i].getShape();
	    assert(allEdges.Contains(edge));
       }

        TopoShape shape(0,getDocument()->getStringHasher());
        shape.makEFillet(baseShape,edges,radius,radius);
//        shape=baseShape;
        if (shape.isNull())
            return new App::DocumentObjectExecReturn("Resulting shape is null");

        TopTools_ListOfShape aLarg;
        aLarg.Append(baseShape.getShape());
        bool failed = false;
        if (!BRepAlgo::IsValid(aLarg, shape.getShape(), Standard_False, Standard_False)) {
            ShapeFix_ShapeTolerance aSFT;
            aSFT.LimitTolerance(shape.getShape(), Precision::Confusion(), Precision::Confusion(), TopAbs_SHAPE);
            Handle(ShapeFix_Shape) aSfs = new ShapeFix_Shape(shape.getShape());
            aSfs->Perform();
            shape.setShape(aSfs->Shape(),false);
            if (!BRepAlgo::IsValid(aLarg, shape.getShape(), Standard_False, Standard_False))
                failed = true;
        }

        if (!failed) {
            shape = refineShapeIfActive(shape);
            shape = getSolid(shape);
        }
        this->Shape.setValue(shape);

        if (failed)
            return new App::DocumentObjectExecReturn("Resulting shape is invalid");
        return App::DocumentObject::StdReturn;
    }
    catch (Standard_Failure& e) {
        return new App::DocumentObjectExecReturn(e.GetMessageString());
    }
}

void Fillet::handleChangedPropertyType(Base::XMLReader &reader, const char * TypeName, App::Property * prop)
{
    if (prop && strcmp(TypeName,"App::PropertyFloatConstraint") == 0 &&
        strcmp(prop->getTypeId().getName(), "App::PropertyQuantityConstraint") == 0) {
        App::PropertyFloatConstraint p;
        p.Restore(reader);
        static_cast<App::PropertyQuantityConstraint*>(prop)->setValue(p.getValue());
    }
    else {
        DressUp::handleChangedPropertyType(reader, TypeName, prop);
    }
}
