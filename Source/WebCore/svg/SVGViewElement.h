/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007 Rob Buis <buis@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#include "SVGAnimatedBoolean.h"
#include "SVGAnimatedPreserveAspectRatio.h"
#include "SVGAnimatedRect.h"
#include "SVGElement.h"
#include "SVGExternalResourcesRequired.h"
#include "SVGFitToViewBox.h"
#include "SVGStringList.h"
#include "SVGZoomAndPan.h"

namespace WebCore {

class SVGViewElement final : public SVGElement,
                             public SVGExternalResourcesRequired,
                             public SVGFitToViewBox,
                             public SVGZoomAndPan {
public:
    static Ref<SVGViewElement> create(const QualifiedName&, Document&);

    using SVGElement::ref;
    using SVGElement::deref;

    SVGStringList& viewTarget() { return m_viewTarget; }
    SVGZoomAndPanType zoomAndPan() const { return m_zoomAndPan; }
    void setZoomAndPan(unsigned short zoomAndPan) { m_zoomAndPan = SVGZoomAndPan::parseFromNumber(zoomAndPan); }

private:
    SVGViewElement(const QualifiedName&, Document&);

    // FIXME: svgAttributeChanged missing.
    void parseAttribute(const QualifiedName&, const AtomicString&) final;

    bool rendererIsNeeded(const RenderStyle&) final { return false; }

    BEGIN_DECLARE_ANIMATED_PROPERTIES(SVGViewElement)
        DECLARE_ANIMATED_BOOLEAN_OVERRIDE(ExternalResourcesRequired, externalResourcesRequired)
        DECLARE_ANIMATED_RECT(ViewBox, viewBox)
        DECLARE_ANIMATED_PRESERVEASPECTRATIO(PreserveAspectRatio, preserveAspectRatio)
    END_DECLARE_ANIMATED_PROPERTIES

    SVGZoomAndPanType m_zoomAndPan;
    SVGStringList m_viewTarget;
};

} // namespace WebCore
