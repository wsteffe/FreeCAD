if(MSVC)
    add_definitions(-DFCAppPart -DHAVE_ACOSH -DHAVE_ASINH -DHAVE_ATANH)
else(MSVC)
    add_definitions(-DHAVE_LIMITS_H -DHAVE_CONFIG_H)
endif(MSVC)

if(FREETYPE_FOUND)
    add_definitions(-DFCUseFreeType)
endif(FREETYPE_FOUND)

include_directories(
    ${CMAKE_BINARY_DIR}
    ${CMAKE_BINARY_DIR}/src
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${Boost_INCLUDE_DIRS}
    ${OCC_INCLUDE_DIR}
    ${PYTHON_INCLUDE_DIRS}
    ${XercesC_INCLUDE_DIRS}
    ${ZLIB_INCLUDE_DIR}
    ${FREETYPE_INCLUDE_DIRS}
)

link_directories(${OCC_LIBRARY_DIR})

set(Part_LIBS 
    ${OCC_LIBRARIES}
    ${OCC_DEBUG_LIBRARIES}
    FreeCADApp
)

if(FREETYPE_FOUND)
    set(Part_LIBS 
        ${Part_LIBS}
        ${FREETYPE_LIBRARIES}
    )
endif(FREETYPE_FOUND)

generate_from_xml(ArcPy)
generate_from_xml(ArcOfConicPy)
generate_from_xml(ArcOfCirclePy)
generate_from_xml(ArcOfParabolaPy)
generate_from_xml(BodyBasePy)
generate_from_xml(ConicPy)
generate_from_xml(CirclePy)
generate_from_xml(ArcOfEllipsePy)
generate_from_xml(EllipsePy)
generate_from_xml(HyperbolaPy)
generate_from_xml(ArcOfHyperbolaPy)
generate_from_xml(ParabolaPy)
generate_from_xml(OffsetCurvePy)
generate_from_xml(GeometryPy)
generate_from_xml(GeometryExtensionPy)
generate_from_xml(GeometryIntExtensionPy)
generate_from_xml(GeometryStringExtensionPy)
generate_from_xml(GeometryBoolExtensionPy)
generate_from_xml(GeometryDoubleExtensionPy)
generate_from_xml(GeometryCurvePy)
generate_from_xml(BoundedCurvePy)
generate_from_xml(TrimmedCurvePy)
generate_from_xml(GeometrySurfacePy)
generate_from_xml(LinePy)
generate_from_xml(LineSegmentPy)
generate_from_xml(PointPy)
generate_from_xml(BezierCurvePy)
generate_from_xml(BSplineCurvePy)
generate_from_xml(PlanePy)
generate_from_xml(ConePy)
generate_from_xml(CylinderPy)
generate_from_xml(SpherePy)
generate_from_xml(ToroidPy)
generate_from_xml(BezierSurfacePy)
generate_from_xml(BSplineSurfacePy)
generate_from_xml(OffsetSurfacePy)
generate_from_xml(PlateSurfacePy)
generate_from_xml(RectangularTrimmedSurfacePy)
generate_from_xml(SurfaceOfExtrusionPy)
generate_from_xml(SurfaceOfRevolutionPy)
generate_from_xml(PartFeaturePy)
generate_from_xml(AttachExtensionPy)
generate_from_xml(Part2DObjectPy)
generate_from_xml(AttachEnginePy)
generate_from_xml(TopoShapePy)
generate_from_xml(TopoShapeCompoundPy)
generate_from_xml(TopoShapeCompSolidPy)
generate_from_xml(TopoShapeEdgePy)
generate_from_xml(TopoShapeFacePy)
generate_from_xml(TopoShapeShellPy)
generate_from_xml(TopoShapeSolidPy)
generate_from_xml(TopoShapeVertexPy)
generate_from_xml(TopoShapeWirePy)
generate_from_xml(BRepOffsetAPI_MakePipeShellPy)
generate_from_xml(BRepOffsetAPI_MakeFillingPy)

# make sure to create the directory at configure time
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/BRepFeat)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/Geom2d)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/GeomPlate)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/ShapeUpgrade)

generate_from_xml(BRepFeat/MakePrismPy)

generate_from_xml(Geom2d/ArcOfCircle2dPy)
generate_from_xml(Geom2d/ArcOfConic2dPy)
generate_from_xml(Geom2d/ArcOfEllipse2dPy)
generate_from_xml(Geom2d/ArcOfHyperbola2dPy)
generate_from_xml(Geom2d/ArcOfParabola2dPy)
generate_from_xml(Geom2d/BezierCurve2dPy)
generate_from_xml(Geom2d/BSplineCurve2dPy)
generate_from_xml(Geom2d/Circle2dPy)
generate_from_xml(Geom2d/Conic2dPy)
generate_from_xml(Geom2d/Ellipse2dPy)
generate_from_xml(Geom2d/Geometry2dPy)
generate_from_xml(Geom2d/Hyperbola2dPy)
generate_from_xml(Geom2d/Curve2dPy)
generate_from_xml(Geom2d/Line2dSegmentPy)
generate_from_xml(Geom2d/Line2dPy)
generate_from_xml(Geom2d/OffsetCurve2dPy)
generate_from_xml(Geom2d/Parabola2dPy)

generate_from_xml(GeomPlate/BuildPlateSurfacePy)
generate_from_xml(GeomPlate/CurveConstraintPy)
generate_from_xml(GeomPlate/PointConstraintPy)

generate_from_xml(ShapeUpgrade/UnifySameDomainPy)

SET(Features_SRCS
    FeaturePartBoolean.cpp
    FeaturePartBoolean.h
    FeaturePartBox.cpp
    FeaturePartBox.h
    FeaturePartCircle.cpp
    FeaturePartCircle.h
    FeaturePartCommon.cpp
    FeaturePartCommon.h
    FeaturePartCurveNet.cpp
    FeaturePartCurveNet.h
    FeaturePartCut.cpp
    FeaturePartCut.h
    FeaturePartFuse.cpp
    FeaturePartFuse.h
    FeaturePartImportBrep.cpp
    FeaturePartImportBrep.h
    FeaturePartImportIges.cpp
    FeaturePartImportIges.h
    FeaturePartImportStep.cpp
    FeaturePartImportStep.h
    FeaturePartPolygon.cpp
    FeaturePartPolygon.h
    FeaturePartSection.cpp
    FeaturePartSection.h
    FeaturePartSpline.cpp
    FeaturePartSpline.h
    FeatureChamfer.cpp
    FeatureChamfer.h
    FeatureCompound.cpp
    FeatureCompound.h
    FeatureExtrusion.cpp
    FeatureExtrusion.h
    FeatureFace.cpp
    FeatureFace.h
    FeatureFillet.cpp
    FeatureFillet.h
    FeatureMirroring.cpp
    FeatureMirroring.h
    FeatureRevolution.cpp
    FeatureRevolution.h
    FeatureOffset.cpp
    FeatureOffset.h
    PartFeatures.cpp
    PartFeatures.h
    PartFeature.cpp
    PartFeature.h
    PartFeatureReference.cpp
    PartFeatureReference.h
    Part2DObject.cpp
    Part2DObject.h
    PrimitiveFeature.cpp
    PrimitiveFeature.h
    FeatureGeometrySet.h
    FeatureGeometrySet.cpp
    CustomFeature.cpp
    CustomFeature.h
    BodyBase.h
    BodyBase.cpp
    DatumFeature.cpp
    DatumFeature.h
    AttachExtension.h
    AttachExtension.cpp
    SubShapeBinder.h
    SubShapeBinder.cpp
    SubShapeBinderPython.cpp
    PartParams.h
    PartParams.cpp
    PrismExtension.cpp
    PrismExtension.h
)
SOURCE_GROUP("Features" FILES ${Features_SRCS})

SET(Properties_SRCS
    PropertyTopoShape.cpp
    PropertyTopoShape.h
    PropertyGeometryList.cpp
    PropertyGeometryList.h
    PropertyDressUp.cpp
    PropertyDressUp.h
)
SOURCE_GROUP("Properties" FILES ${Properties_SRCS})

SET(Python_SRCS
    ArcPy.xml
    ArcPyImp.cpp
    ArcOfConicPy.xml
    ArcOfConicPyImp.cpp
    ArcOfCirclePy.xml
    ArcOfCirclePyImp.cpp
    ArcOfParabolaPy.xml
    ArcOfParabolaPyImp.cpp
    BodyBasePy.xml
    BodyBasePyImp.cpp
    ConicPy.xml
    ConicPyImp.cpp
    CirclePy.xml
    CirclePyImp.cpp
    ArcOfEllipsePy.xml
    ArcOfEllipsePyImp.cpp
    EllipsePy.xml
    EllipsePyImp.cpp
    HyperbolaPy.xml
    HyperbolaPyImp.cpp
    ArcOfHyperbolaPy.xml
    ArcOfHyperbolaPyImp.cpp
    ParabolaPy.xml
    ParabolaPyImp.cpp
    OffsetCurvePy.xml
    OffsetCurvePyImp.cpp
    GeometryPy.xml
    GeometryPyImp.cpp
    GeometryExtensionPy.xml
    GeometryExtensionPyImp.cpp
    GeometryIntExtensionPy.xml
    GeometryIntExtensionPyImp.cpp
    GeometryStringExtensionPy.xml
    GeometryStringExtensionPyImp.cpp
    GeometryBoolExtensionPy.xml
    GeometryBoolExtensionPyImp.cpp
    GeometryDoubleExtensionPy.xml
    GeometryDoubleExtensionPyImp.cpp
    GeometryCurvePy.xml
    GeometryCurvePyImp.cpp
    BoundedCurvePy.xml
    BoundedCurvePyImp.cpp
    TrimmedCurvePy.xml
    TrimmedCurvePyImp.cpp
    GeometrySurfacePy.xml
    GeometrySurfacePyImp.cpp
    LinePy.xml
    LinePyImp.cpp
    LineSegmentPy.xml
    LineSegmentPyImp.cpp
    PointPy.xml
    PointPyImp.cpp
    BezierCurvePy.xml
    BezierCurvePyImp.cpp
    BSplineCurvePy.xml
    BSplineCurvePyImp.cpp
    PlanePy.xml
    PlanePyImp.cpp
    ConePy.xml
    ConePyImp.cpp
    CylinderPy.xml
    CylinderPyImp.cpp
    SpherePy.xml
    SpherePyImp.cpp
    ToroidPy.xml
    ToroidPyImp.cpp
    BezierSurfacePy.xml
    BezierSurfacePyImp.cpp
    BSplineSurfacePy.xml
    BSplineSurfacePyImp.cpp
    OffsetSurfacePy.xml
    OffsetSurfacePyImp.cpp
    PlateSurfacePy.xml
    PlateSurfacePyImp.cpp
    RectangularTrimmedSurfacePy.xml
    RectangularTrimmedSurfacePyImp.cpp
    SurfaceOfExtrusionPy.xml
    SurfaceOfExtrusionPyImp.cpp
    SurfaceOfRevolutionPy.xml
    SurfaceOfRevolutionPyImp.cpp
    PartFeaturePy.xml
    PartFeaturePyImp.cpp
    AttachExtensionPy.xml
    AttachExtensionPyImp.cpp
    Part2DObjectPy.xml
    Part2DObjectPyImp.cpp
    AttachEnginePy.xml
    AttachEnginePyImp.cpp
    TopoShapePy.xml
    TopoShapePyImp.cpp
    TopoShapeCompSolidPy.xml
    TopoShapeCompSolidPyImp.cpp
    TopoShapeCompoundPy.xml
    TopoShapeCompoundPyImp.cpp
    TopoShapeEdgePy.xml
    TopoShapeEdgePyImp.cpp
    TopoShapeFacePy.xml
    TopoShapeFacePyImp.cpp
    TopoShapeShellPy.xml
    TopoShapeShellPyImp.cpp
    TopoShapeSolidPy.xml
    TopoShapeSolidPyImp.cpp
    TopoShapeVertexPy.xml
    TopoShapeVertexPyImp.cpp
    TopoShapeWirePy.xml
    TopoShapeWirePyImp.cpp
    BRepOffsetAPI_MakePipeShellPy.xml
    BRepOffsetAPI_MakePipeShellPyImp.cpp
    BRepOffsetAPI_MakeFillingPy.xml
    BRepOffsetAPI_MakeFillingPyImp.cpp
    PartPyCXX.cpp
    PartPyCXX.h
)
SOURCE_GROUP("Python" FILES ${Python_SRCS})

# BRepFeat wrappers
SET(BRepFeatPy_SRCS
    BRepFeat/MakePrismPy.xml
    BRepFeat/MakePrismPyImp.cpp
)

SOURCE_GROUP("ShapeUpgrade" FILES ${ShapeUpgradePy_SRCS})

# Geom2d wrappers
SET(Geom2dPy_SRCS
    Geom2d/ArcOfCircle2dPy.xml
    Geom2d/ArcOfCircle2dPyImp.cpp
    Geom2d/ArcOfConic2dPy.xml
    Geom2d/ArcOfConic2dPyImp.cpp
    Geom2d/ArcOfEllipse2dPy.xml
    Geom2d/ArcOfEllipse2dPyImp.cpp
    Geom2d/ArcOfHyperbola2dPy.xml
    Geom2d/ArcOfHyperbola2dPyImp.cpp
    Geom2d/ArcOfParabola2dPy.xml
    Geom2d/ArcOfParabola2dPyImp.cpp
    Geom2d/BezierCurve2dPy.xml
    Geom2d/BezierCurve2dPyImp.cpp
    Geom2d/BSplineCurve2dPy.xml
    Geom2d/BSplineCurve2dPyImp.cpp
    Geom2d/Circle2dPy.xml
    Geom2d/Circle2dPyImp.cpp
    Geom2d/Conic2dPy.xml
    Geom2d/Conic2dPyImp.cpp
    Geom2d/Ellipse2dPy.xml
    Geom2d/Ellipse2dPyImp.cpp
    Geom2d/Geometry2dPy.xml
    Geom2d/Geometry2dPyImp.cpp
    Geom2d/Curve2dPy.xml
    Geom2d/Curve2dPyImp.cpp
    Geom2d/Hyperbola2dPy.xml
    Geom2d/Hyperbola2dPyImp.cpp
    Geom2d/Line2dPy.xml
    Geom2d/Line2dPyImp.cpp
    Geom2d/Line2dSegmentPy.xml
    Geom2d/Line2dSegmentPyImp.cpp
    Geom2d/OffsetCurve2dPy.xml
    Geom2d/OffsetCurve2dPyImp.cpp
    Geom2d/Parabola2dPy.xml
    Geom2d/Parabola2dPyImp.cpp
)

SOURCE_GROUP("Geom2d" FILES ${Geom2dPy_SRCS})

# GeomPlate wrappers
SET(GeomPlatePy_SRCS
    GeomPlate/BuildPlateSurfacePy.xml
    GeomPlate/BuildPlateSurfacePyImp.cpp
    GeomPlate/CurveConstraintPy.xml
    GeomPlate/CurveConstraintPyImp.cpp
    GeomPlate/PointConstraintPy.xml
    GeomPlate/PointConstraintPyImp.cpp
)

SOURCE_GROUP("GeomPlate" FILES ${GeomPlatePy_SRCS})

# ShapeUpgrade wrappers
SET(ShapeUpgradePy_SRCS
    ShapeUpgrade/UnifySameDomainPy.xml
    ShapeUpgrade/UnifySameDomainPyImp.cpp
)

SOURCE_GROUP("ShapeUpgrade" FILES ${ShapeUpgradePy_SRCS})

SET(Part_SRCS
    ${Features_SRCS}
    ${Properties_SRCS}
    ${Python_SRCS}
    ${BRepFeatPy_SRCS}
    ${Geom2dPy_SRCS}
    ${GeomPlatePy_SRCS}
    ${ShapeUpgradePy_SRCS}
    Attacher.cpp
    Attacher.h
    AppPart.cpp
    AppPartPy.cpp
    BRepOffsetAPI_MakeOffsetFix.cpp
    BRepOffsetAPI_MakeOffsetFix.h
    BSplineCurveBiArcs.cpp
    CrossSection.cpp
    CrossSection.h
    GeometryExtension.cpp
    GeometryExtension.h
    GeometryDefaultExtension.cpp
    GeometryDefaultExtension.h
    GeometryMigrationExtension.h
    GeometryMigrationExtension.cpp
    Geometry.cpp
    Geometry.h
    Geometry2d.cpp
    Geometry2d.h
    ImportIges.cpp
    ImportIges.h
    ImportStep.cpp
    ImportStep.h
    PreCompiled.cpp
    PreCompiled.h
    ProgressIndicator.cpp
    ProgressIndicator.h
    TopoShape.cpp
    TopoShapeEx.cpp
    TopoShape.h
    TopoShapeOpCode.h
    edgecluster.cpp
    edgecluster.h
    modelRefine.cpp
    modelRefine.h
    Tools.cpp
    Tools.h
    encodeFilename.h
    OCCError.h
    FT2FC.cpp
    FT2FC.h
    FaceMaker.cpp
    FaceMaker.h
    FaceMakerCheese.cpp
    FaceMakerCheese.h
    FaceMakerBullseye.cpp
    FaceMakerBullseye.h
    WireJoiner.cpp
    WireJoiner.h
)

if(FREECAD_USE_PCH)
    add_definitions(-D_PreComp_)
    GET_MSVC_PRECOMPILED_SOURCE("PreCompiled.cpp" PCH_SRCS ${Part_SRCS})
    ADD_MSVC_PRECOMPILED_HEADER(Part PreCompiled.h PreCompiled.cpp PCH_SRCS)
endif(FREECAD_USE_PCH)

# Suppress some very long Eigen3 warnings of older versions
if (EIGEN3_NO_DEPRECATED_COPY)
    set_source_files_properties(
        GeometryCurvePyImp.cpp
        GeometrySurfacePyImp.cpp
        PROPERTIES COMPILE_FLAGS ${EIGEN3_NO_DEPRECATED_COPY})
endif ()

add_library(Part SHARED ${Part_SRCS})
target_link_libraries(Part ${Part_LIBS})


SET_BIN_DIR(Part Part /Mod/Part)
SET_PYTHON_PREFIX_SUFFIX(Part)

INSTALL(TARGETS Part DESTINATION ${CMAKE_INSTALL_LIBDIR})
