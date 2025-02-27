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
# include <QRegExp>
#endif

#include "PovrayHighlighter.h"

using namespace RaytracingGui;

namespace RaytracingGui {
class PovrayHighlighterP
{
public:
    PovrayHighlighterP()
    {
        keywords << QStringLiteral("include") << QStringLiteral("if")
                 << QStringLiteral("ifdef") << QStringLiteral("ifndef")
                 << QStringLiteral("switch") << QStringLiteral("while")
                 << QStringLiteral("macro") << QStringLiteral("else")
                 << QStringLiteral("end") << QStringLiteral("declare")
                 << QStringLiteral("local") << QStringLiteral("undef")
                 << QStringLiteral("fopen") << QStringLiteral("fclose")
                 << QStringLiteral("read") << QStringLiteral("write")
                 << QStringLiteral("default") << QStringLiteral("version")
                 << QStringLiteral("debug") << QStringLiteral("case")
                 << QStringLiteral("range") << QStringLiteral("break")
                 << QStringLiteral("error") << QStringLiteral("warning");
;
    }

    QStringList keywords;
};
} // namespace RaytracingGui

/**
 * Constructs a syntax highlighter.
 */
PovrayHighlighter::PovrayHighlighter(QObject* parent)
    : SyntaxHighlighter(parent)
{
    d = new PovrayHighlighterP;
}

/** Destroys this object. */
PovrayHighlighter::~PovrayHighlighter()
{
    delete d;
}

void PovrayHighlighter::highlightBlock(const QString &text)
{
    enum { NormalState = -1, InsideCStyleComment };
 
    int state = previousBlockState();
    int start = 0;
 
    for (int i = 0; i < text.length(); ++i) {
 
        if (state == InsideCStyleComment) {
            if (text.mid(i, 2) == QStringLiteral("*/")) {
                state = NormalState;
                setFormat(start, i - start + 2, this->colorByType(SyntaxHighlighter::BlockComment));
            }
        }
        else {
            if (text.mid(i, 2) == QStringLiteral("//")) {
                setFormat(i, text.length() - i, this->colorByType(SyntaxHighlighter::Comment));
                break;
            }
            else if (text.mid(i, 2) == QStringLiteral("/*")) {
                start = i;
                state = InsideCStyleComment;
            }
            else if (text.mid(i,1) == QStringLiteral("#")) {
                QRegExp rx(QStringLiteral("#\\s*(\\w*)"));
                int pos = text.indexOf(rx, i);
                if (pos != -1) {
                    if (d->keywords.contains(rx.cap(1)) != 0)
                        setFormat(i, rx.matchedLength(), this->colorByType(SyntaxHighlighter::Keyword));
                    i += rx.matchedLength();
                }
            }
            else if (text[i] == QLatin1Char('"')) {
                int j=i;
                for (;j<text.length();j++) {
                    if (j > i && text[j] == QLatin1Char('"'))
                        break;
                }

                setFormat(i, j-i+1, this->colorByType(SyntaxHighlighter::String));
                i = j;
            }
        }
    }
    if (state == InsideCStyleComment)
        setFormat(start, text.length() - start, this->colorByType(SyntaxHighlighter::BlockComment));
 
    setCurrentBlockState(state);
}
