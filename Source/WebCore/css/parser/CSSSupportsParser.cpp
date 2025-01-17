// Copyright 2015 The Chromium Authors. All rights reserved.
// Copyright (C) 2016 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "config.h"
#include "CSSSupportsParser.h"

#include "CSSParserImpl.h"

namespace WebCore {

CSSSupportsParser::SupportsResult CSSSupportsParser::supportsCondition(CSSParserTokenRange range, CSSParserImpl& parser)
{
    // FIXME: The spec allows leading whitespace in @supports but not CSS.supports,
    // but major browser vendors allow it in CSS.supports also.
    range.consumeWhitespace();
    return CSSSupportsParser(parser).consumeCondition(range);
}

enum ClauseType { Unresolved, Conjunction, Disjunction };

CSSSupportsParser::SupportsResult CSSSupportsParser::consumeCondition(CSSParserTokenRange range)
{
    if (range.peek().type() == IdentToken)
        return consumeNegation(range);

    bool result;
    ClauseType clauseType = Unresolved;

    while (true) {
        SupportsResult nextResult = consumeConditionInParenthesis(range);
        if (nextResult == Invalid)
            return Invalid;
        bool nextSupported = nextResult;
        if (clauseType == Unresolved)
            result = nextSupported;
        else if (clauseType == Conjunction)
            result &= nextSupported;
        else
            result |= nextSupported;

        if (range.atEnd())
            break;
        if (range.consumeIncludingWhitespace().type() != WhitespaceToken)
            return Invalid;
        if (range.atEnd())
            break;

        const CSSParserToken& token = range.consume();
        if (token.type() != IdentToken)
            return Invalid;
        if (clauseType == Unresolved)
            clauseType = token.value().length() == 3 ? Conjunction : Disjunction;
        if ((clauseType == Conjunction && !equalIgnoringASCIICase(token.value(), "and"))
            || (clauseType == Disjunction && !equalIgnoringASCIICase(token.value(), "or")))
            return Invalid;

        if (range.consumeIncludingWhitespace().type() != WhitespaceToken)
            return Invalid;
    }
    return result ? Supported : Unsupported;
}

CSSSupportsParser::SupportsResult CSSSupportsParser::consumeNegation(CSSParserTokenRange range)
{
    ASSERT(range.peek().type() == IdentToken);
    if (!equalIgnoringASCIICase(range.consume().value(), "not"))
        return Invalid;
    if (range.consumeIncludingWhitespace().type() != WhitespaceToken)
        return Invalid;
    SupportsResult result = consumeConditionInParenthesis(range);
    range.consumeWhitespace();
    if (!range.atEnd() || result == Invalid)
        return Invalid;
    return result ? Unsupported : Supported;
}

CSSSupportsParser::SupportsResult CSSSupportsParser::consumeConditionInParenthesis(CSSParserTokenRange& range)
{
    if (range.peek().type() == FunctionToken) {
        range.consumeComponentValue();
        return Unsupported;
    }
    if (range.peek().type() != LeftParenthesisToken)
        return Invalid;
    CSSParserTokenRange innerRange = range.consumeBlock();
    innerRange.consumeWhitespace();
    SupportsResult result = consumeCondition(innerRange);
    if (result != Invalid)
        return result;
    return innerRange.peek().type() == IdentToken && m_parser.supportsDeclaration(innerRange) ? Supported : Unsupported;
}

} // namespace WebCore
