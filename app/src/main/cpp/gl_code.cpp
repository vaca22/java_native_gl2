/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// OpenGL ES 2.0 code

#include <jni.h>
#include <android/log.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define  LOG_TAG    "libgl2jni"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)





#include "esUtil.h"

typedef struct
{
    // Handle to a program object
    GLuint programObject;

    // Attribute locations
    GLint  positionLoc;

    // Uniform locations
    GLint  mvpLoc;

    // Vertex daata
    GLfloat  *vertices;
    GLuint *indices;
    int       numIndices;

    // Rotation angle
    GLfloat   angle;

    // MVP matrix
    ESMatrix  mvpMatrix;
} UserData;


UserData  *userData;



static void printGLString(const char *name, GLenum s) {
    const char *v = (const char *) glGetString(s);
    LOGI("GL %s = %s\n", name, v);
}

static void checkGlError(const char *op) {
    for (GLint error = glGetError(); error; error
                                                    = glGetError()) {
        LOGI("after %s() glError (0x%x)\n", op, error);
    }
}

auto gVertexShader =
        "uniform mat4 u_mvpMatrix;                   \n"
        "attribute vec4 a_position;                  \n"
        "void main()                                 \n"
        "{                                           \n"
        "   gl_Position = u_mvpMatrix * a_position;  \n"
        "}                                           \n";


//        "attribute vec4 vPosition;\n"
//        "void main() {\n"
//        "  gl_Position = vPosition;\n"
//        "}\n";

auto gFragmentShader =
        "precision mediump float;                            \n"
        "void main()                                         \n"
        "{                                                   \n"
        "  gl_FragColor = vec4( 1.0, 0.0, 0.0, 1.0 );        \n"
        "}                                                   \n";
//        "precision mediump float;\n"
//        "void main() {\n"
//        "  gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0);\n"
//        "}\n";

GLuint loadShader(GLenum shaderType, const char *pSource) {
    GLuint shader = glCreateShader(shaderType);
    if (shader) {
        glShaderSource(shader, 1, &pSource, NULL);
        glCompileShader(shader);
        GLint compiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            GLint infoLen = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
            if (infoLen) {
                char *buf = (char *) malloc(infoLen);
                if (buf) {
                    glGetShaderInfoLog(shader, infoLen, NULL, buf);
                    LOGE("Could not compile shader %d:\n%s\n",
                         shaderType, buf);
                    free(buf);
                }
                glDeleteShader(shader);
                shader = 0;
            }
        }
    }
    return shader;
}

GLuint createProgram(const char *pVertexSource, const char *pFragmentSource) {
    GLuint vertexShader = loadShader(GL_VERTEX_SHADER, pVertexSource);
    if (!vertexShader) {
        return 0;
    }

    GLuint pixelShader = loadShader(GL_FRAGMENT_SHADER, pFragmentSource);
    if (!pixelShader) {
        return 0;
    }

    GLuint program = glCreateProgram();
    if (program) {
        glAttachShader(program, vertexShader);
        checkGlError("glAttachShader");
        glAttachShader(program, pixelShader);
        checkGlError("glAttachShader");
        glLinkProgram(program);
        GLint linkStatus = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
        if (linkStatus != GL_TRUE) {
            GLint bufLength = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &bufLength);
            if (bufLength) {
                char *buf = (char *) malloc(bufLength);
                if (buf) {
                    glGetProgramInfoLog(program, bufLength, NULL, buf);
                    LOGE("Could not link program:\n%s\n", buf);
                    free(buf);
                }
            }
            glDeleteProgram(program);
            program = 0;
        }
    }
    return program;
}



bool setupGraphics(int w, int h) {
    printGLString("Version", GL_VERSION);
    printGLString("Vendor", GL_VENDOR);
    printGLString("Renderer", GL_RENDERER);
    printGLString("Extensions", GL_EXTENSIONS);

    LOGI("setupGraphics(%d, %d)", w, h);
    userData->programObject=createProgram(gVertexShader, gFragmentShader);
    if (!userData->programObject) {
        LOGE("Could not create program.");
        return false;
    }
    userData->positionLoc = glGetAttribLocation(userData->programObject, "a_position");
    checkGlError("glGetAttribLocation");
    LOGI("glGetAttribLocation(\"vPosition\") = %d\n",
         userData->positionLoc);
    userData->mvpLoc = glGetUniformLocation( userData->programObject, "u_mvpMatrix" );
    userData->numIndices = esGenCube( 1.0, &userData->vertices,
                                      NULL, NULL, &userData->indices );
    userData->angle = 45.0f;
    glViewport(0, 0, w, h);
    checkGlError("glViewport");
    return true;
}


void Update (  float deltaTime,int width,int height )
{

    ESMatrix perspective;
    ESMatrix modelview;
    float    aspect;

    // Compute a rotation angle based on time to rotate the cube
    userData->angle += ( deltaTime * 40.0f );
    if( userData->angle >= 360.0f )
        userData->angle -= 360.0f;

    // Compute the window aspect ratio
    aspect = (float)width / (float)height;

    // Generate a perspective matrix with a 60 degree FOV
    esMatrixLoadIdentity( &perspective );
    esPerspective( &perspective, 100.0f, aspect, 1.0f, 30.0f );

    // Generate a model view matrix to rotate/translate the cube
    esMatrixLoadIdentity( &modelview );

    // Translate away from the viewer
    esTranslate( &modelview, 0.0, 0.0, -5.0 );

    // Rotate the cube
    esRotate( &modelview, userData->angle, 1.0, 0.0, 1.0 );

    // Compute the final MVP by multiplying the
    // modevleiw and perspective matrices together
    esMatrixMultiply( &userData->mvpMatrix, &modelview, &perspective );
}



void renderFrame() {
    static float grey;

    glClearColor(grey, grey, grey, 1.0f);
    checkGlError("glClearColor");


    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    checkGlError("glClear");

    glUseProgram(userData->programObject);
    checkGlError("glUseProgram");

    glVertexAttribPointer(userData->positionLoc, 3, GL_FLOAT, GL_FALSE, 3*sizeof(GLfloat), userData->vertices);
    checkGlError("glVertexAttribPointer");
    glEnableVertexAttribArray(userData->positionLoc);
    checkGlError("glEnableVertexAttribArray");

    glUniformMatrix4fv( userData->mvpLoc, 1, GL_FALSE, (GLfloat*) &userData->mvpMatrix.m[0][0] );

    glDrawElements ( GL_TRIANGLES, userData->numIndices, GL_UNSIGNED_INT, userData->indices );
    checkGlError("glDrawElements");
}

extern "C" {

JNIEXPORT void JNICALL
Java_com_android_gl2jni_GL2JNILib_init(JNIEnv *env, jobject obj, jint width, jint height);

JNIEXPORT void JNICALL Java_com_android_gl2jni_GL2JNILib_step(JNIEnv *env, jobject obj);
};

int ww=10;
int hh=20;

JNIEXPORT void JNICALL
Java_com_android_gl2jni_GL2JNILib_init(JNIEnv *env, jobject obj, jint width, jint height) {
    userData = static_cast<UserData *>(malloc(sizeof(UserData)));
    setupGraphics(width, height);
    ww=width;
    hh=height;
}

float cc=0;

JNIEXPORT void JNICALL Java_com_android_gl2jni_GL2JNILib_step(JNIEnv *env, jobject obj) {
    renderFrame();
    Update(0.001,ww,hh);

}
