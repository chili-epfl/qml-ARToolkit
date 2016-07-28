import Qt3D.Core 2.0
import Qt3D.Render 2.0
Entity {
    id: sceneRoot

    Camera {
        id: camera
        projectionType: CameraLens.FrustumProjection
        nearPlane : 0.1
        farPlane : 1000

        top: 0.1*(artoolkit.projectionMatrix.m23/artoolkit.projectionMatrix.m11)
        bottom: -0.1*(artoolkit.projectionMatrix.m23/artoolkit.projectionMatrix.m11)
        left: -0.1*(artoolkit.projectionMatrix.m13/artoolkit.projectionMatrix.m22)
        right: 0.1*(artoolkit.projectionMatrix.m13/artoolkit.projectionMatrix.m22)

        position: Qt.vector3d( 0.0, 0.0, -1 )
        upVector: Qt.vector3d( 0.0, -1.0, 0.0 )
        viewCenter: Qt.vector3d( 0.0, 0.0, 0.0 )
    }

    components: [
        FrameGraph {
            activeFrameGraph:TechniqueFilter {
                objectName : "techniqueFilter"
                // Select the forward rendering Technique of any used Effect
                requires: [ Annotation { name: "renderingStyle"; value: "forward" } ]
                // Use the whole viewport
                Viewport {
                    id: viewport
                    objectName : "viewport"
                    rect: Qt.rect(0.0, 0.0, 1.0, 1.0)
                    clearColor: "transparent"

                    // Use the specified camera
                    CameraSelector {
                        id : cameraSelector
                        objectName : "cameraSelector"
                        camera: camera
                        ClearBuffer {
                            buffers : ClearBuffer.ColorDepthBuffer
                            SortMethod {
                                criteria: [
                                    SortCriterion { sort: SortCriterion.StateChangeCost },
                                    SortCriterion { sort: SortCriterion.Material },
                                    SortCriterion { sort: SortCriterion.BackToFront }
                                ]
                            }
                        }
                    }
                }
            }

        }
    ]

    Entity {

        components: [
            Transform {
                    translation:ar_obj.translation
                    rotation:ar_obj.rotationQuaternion
                } ]

        Entity {
            id: sphereEntity
            enabled:ar_obj.objectIsVisible
            CuboidMesh {
                id: cuboidMesh
                xExtent: 50
                yExtent: 50
                zExtent: 30
            }

            Transform {
                id: cuboidTransform
                translation:Qt.vector3d(0,0,15)
            }
            components: [ cuboidMesh, cuboidTransform ]

        }

    }

    Entity {
        components: [
            Transform {
                    translation:ar_obj_multi.translation
                    rotation:ar_obj_multi.rotationQuaternion
                } ]

        Entity {
            id: sphereEntity2
            enabled:ar_obj_multi.objectIsVisible
            CuboidMesh {
                id: cuboidMesh2
                xExtent: 50
                yExtent: 50
                zExtent: 30
            }

            Transform {
                id: cuboidTransform2
                translation:Qt.vector3d(0,0,15)
            }
            components: [ cuboidMesh2, cuboidTransform2 ]

        }

    }

}
