/******************************************************************************
 * Copyright 2003 Jonathan Cohen, Nat Duca, David Luebke, Brenden Schubert    *
 *                Johns Hopkins University and University of Virginia         *
 ******************************************************************************
 * This file is distributed as part of the GLOD library, and as such, falls   *
 * under the terms of the GLOD public license. GLOD is distributed without    *
 * any warranty, implied or otherwise. See the GLOD license for more details. *
 *                                                                            *
 * You should have recieved a copy of the GLOD Open-Source License with this  *
 * copy of GLOD; if not, please visit the GLOD web page,                      *
 * http://www.cs.jhu.edu/~graphics/GLOD/license for more information          *
 ******************************************************************************/

/* This is the exportable header file for the use of GLOD with GL
 ****************************************************************************/

#ifndef GLODAPI_H
#define GLODAPI_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#ifdef GLOD
#define GLOD_APIENTRY __declspec(dllexport)
#else
#define GLOD_APIENTRY __declspec(dllimport)
#endif
#else
#define GLOD_APIENTRY
#endif

/* GLOD Errors
 ***************************************************************************/
#define GLOD_NO_ERROR             0x000
#define GLOD_INVALID_NAME         0x001
#define GLOD_INVALID_DATA_FORMAT  0x002
#define GLOD_INVALID_STATE        0x003
#define GLOD_INVALID_PATCH        0x004
#define GLOD_UNKNOWN_PROPERTY     0x005
#define GLOD_UNSUPPORTED_PROPERTY 0x006
#define GLOD_INVALID_PARAM        0x007

#define GLOD_BAD_HIERARCHY        0x020

#define GLOD_BAD_MAGIC            0x050
#define GLOD_CORRUPT_BUFFER       0x051


/* NewObject Formats 
 ***************************************************************************/
/// reserved for glod_core.h::GLOD_FORMAT_UNKNOWN
#define GLOD_RESERVED                   0x0000 

#define GLOD_CONTINUOUS                 0x0001
#define GLOD_DISCRETE                   0x0002
#define GLOD_DISCRETE_MANUAL            0x0003
#define GLOD_DISCRETE_PATCH             0x0004
#define GLOD_VDS                        0x0001
        
/* Object Param Names
 ***************************************************************************/
#define GLOD_READBACK_SIZE         0x01
#define GLOD_NUM_PATCHES           0x02
#define GLOD_PATCH_NAMES           0x03
#define GLOD_PATCH_SIZES           0x04
#define GLOD_XFORM_MATRIX          0x05

#define GLOD_BUILD_OPERATOR        0x20
#define GLOD_BUILD_QUEUE_MODE      0x21
#define GLOD_BUILD_ERROR_METRIC    0x22
#define GLOD_BUILD_SHARE_TOLERANCE 0x23
#define GLOD_BUILD_BORDER_MODE     0x24
#define GLOD_BUILD_SNAPSHOT_MODE   0x25
#define GLOD_BUILD_PERCENT_REDUCTION_FACTOR 0x26
#define GLOD_BUILD_TRI_SPECS       0x27
#define GLOD_BUILD_ERROR_SPECS     0x28
#define GLOD_BUILD_PERMISSION_GRID_PRECISION 0x29
#define GLOD_QUADRIC_MULTIPLIER	   0x2a
    
#define GLOD_XFORM                 0x41
#define GLOD_APPLY_OBJECT_XFORM    0x42
#define GLOD_IMPORTANCE            0x50

/* Object::Possible Param Values
 ***************************************************************************/
#define GLOD_OPERATOR_MANUAL             0x00
#define GLOD_OPERATOR_HALF_EDGE_COLLAPSE 0x01
#define GLOD_OPERATOR_EDGE_COLLAPSE      0x02
#define GLOD_OPERATOR_VERTEX_PAIR        0x03
#define GLOD_OPERATOR_VERTEX_CLUSTER     0x04

#define GLOD_QUEUE_GREEDY                0x01
#define GLOD_QUEUE_LAZY                  0x02
#define GLOD_QUEUE_INDEPENDENT           0x03
#define GLOD_QUEUE_RANDOMIZED            0x04

#define GLOD_METRIC_SPHERES              0x01
#define GLOD_METRIC_QUADRICS             0x02
#define GLOD_METRIC_PERMISSION_GRID      0x03

#define GLOD_BORDER_UNLOCK               0x01
#define GLOD_BORDER_LOCK                 0x02

#define GLOD_SNAPSHOT_PERCENT_REDUCTION  0x01
#define GLOD_SNAPSHOT_TRI_SPEC           0x02
#define GLOD_SNAPSHOT_ERROR_SPEC         0x03

/* GLOD Group Params
 ***************************************************************************/
#define GLOD_ADAPT_MODE                   0x01
#define GLOD_ERROR_MODE                   0x02
#define GLOD_OBJECT_SPACE_ERROR_THRESHOLD 0x03
#define GLOD_SCREEN_SPACE_ERROR_THRESHOLD 0x04
#define GLOD_MAX_TRIANGLES                0x05

/* Group::Possible Param Values
 ***************************************************************************/
#define GLOD_ERROR_THRESHOLD    0x01
#define GLOD_TRIANGLE_BUDGET    0x02
#define GLOD_OBJECT_SPACE_ERROR 0x03
#define GLOD_SCREEN_SPACE_ERROR 0x04

GLOD_APIENTRY GLuint glodInit( );
GLOD_APIENTRY void glodShutdown( );

GLOD_APIENTRY GLuint glodGetError( void );

GLOD_APIENTRY void glodLoadObject( GLuint name, GLuint groupname, 
                                   const GLvoid *data );
GLOD_APIENTRY void glodReadbackObject( GLuint name, GLvoid *data );
GLOD_APIENTRY void glodFillArrays( GLuint object_name, GLuint patch_name );
GLOD_APIENTRY void glodFillElements( GLuint object_name, GLuint patch_name, 
                                     GLenum type, GLvoid* out_elements );


GLOD_APIENTRY void glodInsertArrays( GLuint name, GLuint patchname, 
                                     GLenum mode, GLint first, GLsizei count,
                                     GLuint level, GLfloat geometric_error );
GLOD_APIENTRY void glodInsertElements( GLuint name, GLuint patchname, 
                                       GLenum mode, GLuint count, GLenum type, GLvoid*indices, 
                                       GLuint level, GLfloat geometric_error );


GLOD_APIENTRY void glodInstanceObject( GLuint name, GLuint instancename, 
                                       GLuint groupname );
GLOD_APIENTRY void glodNewObject( GLuint name, GLuint groupname,
                                  GLenum format );
GLOD_APIENTRY void glodBuildObject( GLuint name );
GLOD_APIENTRY void glodDeleteObject( GLuint name );

GLOD_APIENTRY void glodDrawPatch( GLuint name, GLuint patchname );

GLOD_APIENTRY void glodSetLayout(int rows, int cols);

GLOD_APIENTRY void glodNewGroup( GLuint groupname );
GLOD_APIENTRY void glodAdaptGroup( GLuint groupname );
GLOD_APIENTRY void glodObjectXform( GLuint object_name, float m1[16],
                                    float m2[16], float m3[16] );
GLOD_APIENTRY void glodBindObjectXform( GLuint object_name, GLenum which );
GLOD_APIENTRY void glodDeleteGroup( GLuint groupname );

GLOD_APIENTRY void glodObjectParameterf( GLuint name, GLenum pname,
                                         GLfloat param );
GLOD_APIENTRY void glodObjectParameteri( GLuint name, GLenum pname,
                                         GLint param );
GLOD_APIENTRY void glodObjectParameteriv( GLuint name, GLenum pname,
                                          GLint count, GLint *param );
GLOD_APIENTRY void glodObjectParameterfv( GLuint name, GLenum pname,
                                          GLint count, GLfloat *param );
GLOD_APIENTRY void glodPatchParameterf( GLuint name, GLuint patch_name,
                                        GLenum pname, GLfloat param );
GLOD_APIENTRY void glodPatchParameteri( GLuint name, GLuint patch_name,
                                        GLenum pname, GLint param );
GLOD_APIENTRY void glodGetGroupParameterfv( GLuint groupname, GLenum pname,
                                            GLfloat *param );
GLOD_APIENTRY void glodGetGroupParameteriv( GLuint groupname, GLenum pname,
                                            GLint *param );
GLOD_APIENTRY void glodGetObjectParameterfv( GLuint groupname, GLenum pname,
                                             GLfloat *param );
GLOD_APIENTRY void glodGetObjectParameteriv( GLuint groupname, GLenum pname,
                                             GLint *param );
GLOD_APIENTRY void glodGetPatchParameterfv( GLuint name, GLuint patch_name,
                                            GLenum pname, GLfloat*param );
GLOD_APIENTRY void glodGetPatchParameteriv( GLuint name, GLuint patch_name,
                                            GLenum pname, GLint*param );
GLOD_APIENTRY void glodGroupParameterf( GLuint groupname, GLenum pname,
                                        GLfloat param );
GLOD_APIENTRY void glodGroupParameteri( GLuint groupname, GLenum pname,
                                        GLint param );

GLOD_APIENTRY void glodDebugDrawObject( GLuint name ); /* debugging only */

#ifdef __cplusplus
} /* extern c */
#endif
#endif /* GLODAPI_H */

