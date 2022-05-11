#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <vector>
#include <GL/glew.h>					
#include <GLFW/glfw3.h>
#include "glm/glm.hpp"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/transform.hpp"
#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/string_cast.hpp"
#include "glm/gtc/type_ptr.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

using namespace std;

// Struct for holding vertex data
struct Vertex {
	glm::vec3 position;
	glm::vec4 color;
	glm::vec3 normal;
	glm::vec2 texcoords;
	glm::vec3 tangent;
};

// Struct for holding mesh data
struct Mesh {
	vector<Vertex> vertices;
	vector<unsigned int> indices;
};

// Struct for holding OpenGL mesh
struct MeshGL {
	GLuint VBO = 0;
	GLuint EBO = 0;
	GLuint VAO = 0;
	int indexCnt = 0;
};

// Struct for holding PointLight mesh
struct PointLight {
	glm::vec4 pos;
	glm::vec4 color;
};

//Global Variables
float rotAngle = 0.0;
PointLight light;

glm::vec3 eye(0,0,1);
glm::vec3 lookAt(0,0,0);

glm::vec2 mousePos;

float metallic = 0.0;
float roughness = 0.1;

//Debugging Functions
void aiMatToGLM4(aiMatrix4x4 &a, glm::mat4 &m) {
	for(int i = 0; i < 4; i++) {
		for(int j = 0; j < 4; j++) {
			m[j][i] = a[i][j];
		}
	}
}

void printTab(int cnt) {
	for(int i = 0; i < cnt; i++) {
		cout << "\t";
	}
}
void printNodeInfo(aiNode *node, glm::mat4 &nodeT, glm::mat4 &parentMat, glm::mat4 &currentMat, int level) {
	printTab(level);
	cout << "NAME: " << node->mName.C_Str() << endl;
	printTab(level);
	cout << "NUM MESHES: " << node->mNumMeshes << endl;
	printTab(level);
	cout << "NUM CHILDREN: " << node->mNumChildren << endl;
	printTab(level);
	cout << "Parent Model Matrix:" << glm::to_string(parentMat) << endl;
	printTab(level);
	cout << "Node Transforms:" << glm::to_string(nodeT) << endl;
	printTab(level);
	cout << "Current Model Matrix:" << glm::to_string(currentMat) << endl;
	cout << endl;
}

// Read from file and dump in string
string readFileToString(string filename) {
	// Open file
	ifstream file(filename);
	// Could we open file?
	if(!file || file.fail()) {
		cerr << "ERROR: Could not open file: " << filename << endl;
		const char *m = ("ERROR: Could not open file: " + filename).c_str();
		throw runtime_error(m);
	}

	// Create output stream to receive file data
	ostringstream outS;
	outS << file.rdbuf();
	// Get actual string of file contents
	string allS = outS.str();
	// Close file
	file.close();
	// Return string
	return allS;
}

// Print out shader code
void printShaderCode(string &vertexCode, string &fragCode) {
	cout << "***********************" << endl;
	cout <<"** VERTEX SHADER CODE **" << endl;
	cout << "***********************" << endl;
	cout << vertexCode << endl;
	cout << "*************************" << endl;
	cout <<"** FRAGMENT SHADER CODE **" << endl;
	cout << "*************************" << endl;
	cout << fragCode << endl;
	cout << "*************************" << endl;
}

// GLFW error callback
static void error_callback(int error, const char* description) {
	cerr << "ERROR " << error << ": " << description << endl;
}

// GLFW setup
GLFWwindow* setupGLFW(int major, int minor, int windowWidth, int windowHeight, bool debugging) {
	
	// Set GLFW error callback
	glfwSetErrorCallback(error_callback);

	// (Try to) initialize GLFW
	if (!glfwInit()) {
		exit(EXIT_FAILURE);
	}

	// Force specific OpenGL version
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, major);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, minor);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

	// Request debugging context?
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, debugging);
	
	// Create our GLFW window
	GLFWwindow* window = glfwCreateWindow(	windowWidth, windowHeight, 
											"SomethingOtherThanBasicGraphics", 
											NULL, NULL);

	// Were we able to make it?
	if (!window) {
		// Kill GLFW and exit program
		glfwTerminate();
		exit(EXIT_FAILURE);
	}

	// We want to draw to this window, so make the OpenGL context associated with this window our current context.
	glfwMakeContextCurrent(window);

	// Basically, turning VSync on (so we will wait until the screen is updated once before swapping the back and front buffers
	glfwSwapInterval(1);

	// Return window
	return window;
}

// Cleanup GLFW
void cleanupGLFW(GLFWwindow* window) {
	glfwDestroyWindow(window);
	glfwTerminate();
}

// GLEW setup
void setupGLEW(GLFWwindow* window) {
	
	// MAC-SPECIFIC: Some issues occur with using OpenGL core and GLEW; so, we'll use the experimental version of GLEW
	glewExperimental = true;

	// (Try to) initalize GLEW
	GLenum err = glewInit();

	if (GLEW_OK != err) {
		// We couldn't start GLEW, so we've got to go.
		// Kill GLFW and get out of here
		cout << "ERROR: GLEW could not start: " << glewGetErrorString(err) << endl;
		cleanupGLFW(window);
		exit(EXIT_FAILURE);
	}

	cout << "GLEW initialized; version ";
	cout << glewGetString(GLEW_VERSION) << endl;
}

// Check OpenGL version
void checkOpenGLVersion() {
	GLint glMajor, glMinor;
	glGetIntegerv(GL_MAJOR_VERSION, &glMajor);
	glGetIntegerv(GL_MINOR_VERSION, &glMinor);
	cout << "OpenGL context version: ";
	cout << glMajor << "." << glMinor << endl;
	cout << "Supported GLSL version is ";
	cout << glGetString(GL_SHADING_LANGUAGE_VERSION) << endl;
}

// OpenGL debugging callback
// Taken from: https://learnopengl.com/In-Practice/Debugging
void APIENTRY openGLDebugCallback(	GLenum source, 
									GLenum type, 
									unsigned int id, 
									GLenum severity, 
									GLsizei length, 
									const char *message, 
									const void *userParam) {
    // ignore non-significant error/warning codes
    if(id == 131169 || id == 131185 || id == 131218 || id == 131204) return; 

    cout << "---------------" << endl;
    cout << "Debug message (" << id << "): " <<  message << endl;

    switch (source)
    {
        case GL_DEBUG_SOURCE_API:             cout << "Source: API"; break;
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   cout << "Source: Window System"; break;
        case GL_DEBUG_SOURCE_SHADER_COMPILER: cout << "Source: Shader Compiler"; break;
        case GL_DEBUG_SOURCE_THIRD_PARTY:     cout << "Source: Third Party"; break;
        case GL_DEBUG_SOURCE_APPLICATION:     cout << "Source: Application"; break;
        case GL_DEBUG_SOURCE_OTHER:           cout << "Source: Other"; break;
    } std::cout << std::endl;

    switch (type)
    {
        case GL_DEBUG_TYPE_ERROR:               cout << "Type: Error"; break;
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: cout << "Type: Deprecated Behaviour"; break;
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  cout << "Type: Undefined Behaviour"; break; 
        case GL_DEBUG_TYPE_PORTABILITY:         cout << "Type: Portability"; break;
        case GL_DEBUG_TYPE_PERFORMANCE:         cout << "Type: Performance"; break;
        case GL_DEBUG_TYPE_MARKER:              cout << "Type: Marker"; break;
        case GL_DEBUG_TYPE_PUSH_GROUP:          cout << "Type: Push Group"; break;
        case GL_DEBUG_TYPE_POP_GROUP:           cout << "Type: Pop Group"; break;
        case GL_DEBUG_TYPE_OTHER:               cout << "Type: Other"; break;
    } std::cout << std::endl;
    
    switch (severity)
    {
        case GL_DEBUG_SEVERITY_HIGH:         cout << "Severity: high"; break;
        case GL_DEBUG_SEVERITY_MEDIUM:       cout << "Severity: medium"; break;
        case GL_DEBUG_SEVERITY_LOW:          cout << "Severity: low"; break;
        case GL_DEBUG_SEVERITY_NOTIFICATION: cout << "Severity: notification"; break;
    } cout << endl;
    cout << endl;
}

// Check and setup debugging
void checkAndSetupOpenGLDebugging() {
	// If we have a debug context, we can connect a callback function for OpenGL errors...
	int flags; 
	glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
	if(flags & GL_CONTEXT_FLAG_DEBUG_BIT) {
		// Enable debug output
		glEnable(GL_DEBUG_OUTPUT);
		// Call debug output function when error occurs
    	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS); 
		// Attach error callback
    	glDebugMessageCallback(openGLDebugCallback, nullptr);
		// Control output
		// * ALL messages...
    	glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
		// * Only high severity errors from the OpenGL API...
		// glDebugMessageControl(GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_ERROR, GL_DEBUG_SEVERITY_HIGH, 0, nullptr, GL_TRUE); 
	}
}

// GLSL Compiling/Linking Error Check
// Returns GL_TRUE if compile was successful; GL_FALSE otherwise.
GLint checkGLSLError(GLuint ID, bool isCompile) {

	GLint resultGL = GL_FALSE;
	int infoLogLength;
	char *errorMessage = nullptr;

	if(isCompile) {
		// Get the compilation status and message length
		glGetShaderiv(ID, GL_COMPILE_STATUS, &resultGL);	
		glGetShaderiv(ID, GL_INFO_LOG_LENGTH, &infoLogLength);
	}
	else {
		// Get linking status and message length
		glGetProgramiv(ID, GL_LINK_STATUS, &resultGL);
		glGetProgramiv(ID, GL_INFO_LOG_LENGTH, &infoLogLength);
	}

	// Make sure length is at least one and allocate space for message	
	infoLogLength = (infoLogLength > 1) ? infoLogLength : 1;
	errorMessage = new char[infoLogLength];	

	// Get actual message
	if(isCompile)
		glGetShaderInfoLog(ID, infoLogLength, NULL, errorMessage);		
	else	
		glGetProgramInfoLog(ID, infoLogLength, NULL, errorMessage);

	// Print error message
	if(infoLogLength > 1)
		cout << errorMessage << endl;

	// Cleanup
	if(errorMessage) delete [] errorMessage;

	// Return OpenGL error
	return resultGL;
}

// Creates and compiles GLSL shader from code string; returns shader ID
GLuint createAndCompileShader(const char *shaderCode, GLenum shaderType) {
	// Create the shader ID
	GLuint shaderID = glCreateShader(shaderType);

	// Compile the vertex shader...
	cout << "Compiling shader..." << endl;
	glShaderSource(shaderID, 1, &shaderCode, NULL);
	glCompileShader(shaderID);

	// Checking result of compilation...
	GLint compileOK = checkGLSLError(shaderID, true);
	if (!compileOK || shaderID == 0) {
		glDeleteShader(shaderID);		
		cout << "Error compiling shader." << endl;
		throw runtime_error("Error compiling shader.");
	}

	// Return shader ID
	return shaderID;
}

// Given a list of compiled shaders, create and link a shader program (ID returned).
GLuint createAndLinkShaderProgram(std::vector<GLuint> allShaderIDs) {

	// Create program ID and attach shaders
	cout << "Linking program..." << endl;
	GLuint programID = glCreateProgram();
	for (GLuint &shaderID : allShaderIDs) {
		glAttachShader(programID, shaderID);
	}

	// Actually link the program
	glLinkProgram(programID);

	// Detach shaders (program already linked, successful or not)
	for (GLuint &shaderID : allShaderIDs) {
		glDetachShader(programID, shaderID);		
	}

	// Check linking
	GLint linkOK = checkGLSLError(programID, false);
	if (!linkOK || programID == 0) {		
		glDeleteProgram(programID);		
		cout << "Error linking shaders." << endl;
		throw runtime_error("Error linking shaders.");
	}

	// Return program ID
	return programID;
}

// Does the following:
// - Creates and compiles vertex and fragment shaders (from provided code strings)
// - Creates and links shader program
// - Deletes vertex and fragment shaders
GLuint initShaderProgramFromSource(string vertexShaderCode, string fragmentShaderCode) {
	GLuint vertID = 0;
	GLuint fragID = 0;
	GLuint programID = 0;

	try {
		// Create and compile shaders
		cout << "Vertex shader: ";
		vertID = createAndCompileShader(vertexShaderCode.c_str(), GL_VERTEX_SHADER);
		cout << "Fragment shader: ";
		fragID = createAndCompileShader(fragmentShaderCode.c_str(), GL_FRAGMENT_SHADER);

		// Create and link program
		programID = createAndLinkShaderProgram({ vertID, fragID });

		// Delete individual shaders
		glDeleteShader(vertID);
		glDeleteShader(fragID);

		// Success!
		cout << "Program successfully compiled and linked!" << endl;
	}
	catch (exception e) {
		// Cleanup shaders and shader program, just in case
		if (vertID) glDeleteShader(vertID);
		if (fragID) glDeleteShader(fragID);		
		// Rethrow exception
		throw e;
	}

	return programID;
}

// Create very simple mesh: a quad (4 vertices, 6 indices, 2 triangles)
void createSimpleQuad(Mesh &m) {
	// Clear out vertices and elements
	m.vertices.clear();
	m.indices.clear();

	// Create four corners
	Vertex upperLeft, upperRight;
	Vertex lowerLeft, lowerRight;
	Vertex farLeft;

	// Set positions of vertices
	// Note: glm::vec3(x, y, z)
	upperLeft.position = glm::vec3(-0.5, 0.5, 0.0);
	upperRight.position = glm::vec3(0.5, 0.5, 0.0);
	lowerLeft.position = glm::vec3(-0.5, -0.5, 0.0);
	lowerRight.position = glm::vec3(0.5, -0.5, 0.0);
	farLeft.position = glm::vec3(-0.75, 0.0, 0.0);

	// Set vertex colors (red, green, blue, white)
	// Note: glm::vec4(red, green, blue, alpha)
	upperLeft.color = glm::vec4(1.0, 0.0, 0.0, 1.0);
	upperRight.color = glm::vec4(0.0, 1.0, 0.0, 1.0);
	lowerLeft.color = glm::vec4(0.0, 0.0, 1.0, 1.0);
	lowerRight.color = glm::vec4(1.0, 1.0, 1.0, 1.0);
	farLeft.color = glm::vec4(0.5, 0.7, 0.2, 1.0);

	// Add to mesh's list of vertices
	m.vertices.push_back(upperLeft);
	m.vertices.push_back(upperRight);	
	m.vertices.push_back(lowerLeft);
	m.vertices.push_back(lowerRight);
	m.vertices.push_back(farLeft);
	
	// Add indices for two triangles
	m.indices.push_back(0);
	m.indices.push_back(3);
	m.indices.push_back(1);

	m.indices.push_back(0);
	m.indices.push_back(2);
	m.indices.push_back(3);

	m.indices.push_back(4);
	m.indices.push_back(2);
	m.indices.push_back(0);
}

// Rotation
glm::mat4 makeRotateZ(glm::vec3 offset) {
	glm::mat4 transform = glm::translate(-offset);
	transform = glm::rotate(glm::radians(rotAngle), glm::vec3(0,0,1)) * transform;
	transform = glm::translate(offset) * transform;
	return transform;
}

glm::mat4 makeLocalRotate(glm::vec3 offset, glm::vec3 axis, float angle) {
	glm::mat4 transform = glm::translate(-offset);
	transform = glm::rotate(glm::radians(angle), axis) * transform;
	transform = glm::translate(offset) * transform;
	return transform;
}

//Mouse Callback
static void mouse_position_callback(GLFWwindow* window, double xpos, double ypos) {
	//cout << mousePos[0] << " " << mousePos[1] << endl;
	//cout << xpos << " " << ypos << endl;
	
	double xmov = mousePos[0] - xpos;
	double ymov = mousePos[1] - ypos;

	//if(xmov > 1 || ymov > 1) {
	//	xmov = 0;
	//	ymov = 0;
	//}

	//cout << xmov << " " << ymov << endl;

	int fw, fh;
    glfwGetFramebufferSize(window, &fw, &fh);
	if(fw > 0 && fh > 0) {
        double relxpos = xmov / fw;
        double relypos = ymov / fh;
        
		glm::mat4 relxmot = makeLocalRotate(eye, glm::vec3(0,1,0), 30.0 * relxpos);
		glm::mat4 relymot = makeLocalRotate(eye, glm::cross(glm::vec3(0,1,0), eye - lookAt), 30.0 * relypos);

		glm::vec4 lookAtV = glm::vec4(lookAt, 1.0);
		lookAtV = relymot * relxmot * lookAtV;
		lookAt = glm::vec3(lookAtV);
    }

	mousePos[0] = xpos;
	mousePos[1] = ypos;
	//glfwSetCursorPos(window, 0, 0); 
}

// Key Callbacks
static void key_callback(GLFWwindow *window,
                        int key,
                        int scancode,
                        int action,
                        int mods) {
    if(action == GLFW_PRESS || action == GLFW_REPEAT) {
        if(key == GLFW_KEY_ESCAPE) {
            glfwSetWindowShouldClose(window, true);
        }
		else if(key == GLFW_KEY_W) {
			glm::vec3 direction = (lookAt - eye) * glm::vec3(0.1, 0.1, 0.1);
			lookAt += direction;
			eye += direction;
		}
		else if(key == GLFW_KEY_A) {
			glm::vec3 direction = (glm::cross(eye - lookAt, glm::vec3(0,1,0))) * glm::vec3(0.1, 0.1, 0.1);
			lookAt += direction;
			eye += direction;
		}
		else if(key == GLFW_KEY_S) {
			glm::vec3 direction = (eye - lookAt) * glm::vec3(0.1, 0.1, 0.1);
			lookAt += direction;
			eye += direction;
		}
		else if(key == GLFW_KEY_D) {
			glm::vec3 direction = (glm::cross(glm::vec3(0,1,0), eye - lookAt)) * glm::vec3(0.1, 0.1, 0.1);
			lookAt += direction;
			eye += direction;
		}
        else if(key == GLFW_KEY_J) {
			rotAngle += 1.0;
        }
        else if(key == GLFW_KEY_K) {
			rotAngle -= 1.0;
        }
		else if(key == GLFW_KEY_1) {
			light.color = glm::vec4(1.0,1.0,1.0,1.0);
		}
		else if(key == GLFW_KEY_2) {
			light.color = glm::vec4(1.0,0.0,0.0,1.0);
		}
		else if(key == GLFW_KEY_3) {
			light.color = glm::vec4(0.0,1.0,0.0,1.0);
		}
		else if(key == GLFW_KEY_4) {
			light.color = glm::vec4(0.0,0.0,1.0,1.0);
		}
		else if(key == GLFW_KEY_V) {
			metallic = max(0.0, metallic - 0.1);
		}
		else if(key == GLFW_KEY_B) {
			metallic = min(1.0, metallic + 0.1);
		}
		else if(key == GLFW_KEY_N) {
			roughness = max(0.1, roughness - 0.1);
		}
		else if(key == GLFW_KEY_M) {
			roughness = min(0.7, roughness + 0.1);
		}
    }
}

void extractMeshData(aiMesh *mesh, Mesh &m) {
	// Clear out vertices and elements
	m.vertices.clear();
	m.indices.clear();
	
	// Loop through meshes
	for (int i = 0; i < mesh->mNumVertices; i++) {
		Vertex v;
		v.position = glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
		v.color = glm::vec4(1.0, 1.0, 0.0, 1.0);
		v.normal = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
		v.texcoords = glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
		v.tangent = glm::vec3(mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z);

		m.vertices.push_back(v);
	}
	
	// Loop through faces
	for (int i = 0; i < mesh->mNumFaces; i++) {
		aiFace face;
		face = mesh->mFaces[i];
		for (int k = 0; k < face.mNumIndices; k++) {
			m.indices.push_back(face.mIndices[k]);
		}
	}
}

// Create OpenGL mesh (VAO) from mesh data
void createMeshGL(Mesh &m, MeshGL &mgl) {
	// Create Vertex Buffer Object (VBO)
	glGenBuffers(1, &(mgl.VBO));
	glBindBuffer(GL_ARRAY_BUFFER, mgl.VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex)*m.vertices.size(), m.vertices.data(), GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	
	// Create Vertex Array Object (VAO)
	glGenVertexArrays(1, &(mgl.VAO));

	// Enable VAO
	glBindVertexArray(mgl.VAO);

	// Enable the first two vertex attribute arrays
	glEnableVertexAttribArray(0);	// position
	glEnableVertexAttribArray(1);	// color
	glEnableVertexAttribArray(2);   // normal
	glEnableVertexAttribArray(3);   // texcoords
	glEnableVertexAttribArray(4);   // tangent

	
	// Bind the VBO and set up data mappings so that VAO knows how to read it
	// 0 = pos (3 elements)
	// 1 = color (4 elements)
	glBindBuffer(GL_ARRAY_BUFFER, mgl.VBO);	

	// Attribute, # of components, type, normalized?, stride, array buffer offset
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
	glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texcoords));
	glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, tangent));

	
	// Create Element Buffer Object (EBO)
	glGenBuffers(1, &(mgl.EBO));
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mgl.EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER,
		m.indices.size() * sizeof(GLuint),
		m.indices.data(),
		GL_STATIC_DRAW);

	// Set index count
	mgl.indexCnt = (int)m.indices.size();

	// Unbind vertex array for now
	glBindVertexArray(0);
}

// Draw OpenGL mesh
void drawMesh(MeshGL &mgl) {
	glBindVertexArray(mgl.VAO);
	glDrawElements(GL_TRIANGLES, mgl.indexCnt, GL_UNSIGNED_INT, (void*)0);
	glBindVertexArray(0);		
}

unsigned int loadAndCreateTexture(string filename) {
    int twidth, theight, tnumc;
    stbi_set_flip_vertically_on_load(1);
    unsigned char* tex_image = stbi_load(filename.c_str(), &twidth, &theight, &tnumc, 0);

    if(!tex_image) {
        cout << "COULD NOT LOAD TEXTURE: " << filename << endl;
        glfwTerminate();
        exit(1);
    }

    GLenum format;
    if(tnumc == 3) {
        format = GL_RGB;
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    }
    else if(tnumc == 4) {
        format = GL_RGBA;
    }
    else {
        cout << "UNKNOWN NUMBER OF CHANNELS: " << tnumc << endl;
        glfwTerminate();
        exit(1);
    }

    unsigned int textureID = 0;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, format, twidth, theight, 0, format, 
                    GL_UNSIGNED_BYTE, tex_image);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    stbi_image_free(tex_image);

    return textureID;
}

void renderScene(vector<MeshGL> &allMeshes,
				aiNode *node,
				glm::mat4 parentMat,
				GLint modelMatLoc,
				GLint normMatLoc,
				glm::mat4 viewMat,
				int level) {
	aiMatrix4x4 transformation = node->mTransformation;
	glm::mat4 nodeT;
	aiMatToGLM4(transformation, nodeT);
	glm::mat4 modelMat = parentMat*nodeT;
	glm::mat4 R = makeRotateZ(modelMat[3]);

	glm::mat4 tmpModel = R * modelMat;
	glUniformMatrix4fv(modelMatLoc, 1, false, glm::value_ptr(tmpModel));

	glm::mat3 normMat = glm::transpose(glm::inverse(glm::mat3(viewMat * tmpModel)));
	glUniformMatrix3fv(normMatLoc, 1, false, glm::value_ptr(normMat));
	for(int i = 0; i < node->mNumMeshes; i++){
		int index = node->mMeshes[i];
		drawMesh(allMeshes.at(index));
	}
	for(int i = 0; i < node->mNumChildren; i++){
		renderScene(allMeshes, node->mChildren[i], modelMat, modelMatLoc, normMatLoc, viewMat, level + 1);
	}
}

// Cleanup OpenGL mesh
void cleanupMesh(MeshGL &mgl) {

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glDeleteBuffers(1, &(mgl.VBO));
	mgl.VBO = 0;

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glDeleteBuffers(1, &(mgl.EBO));
	mgl.EBO = 0;

	glBindVertexArray(0);
	glDeleteVertexArrays(1, &(mgl.VAO));
	mgl.VAO = 0;

	mgl.indexCnt = 0;
}

// Main 
int main(int argc, char **argv) {

	// Check if there is an argument
	if (argc <= 1) {
		throw std::invalid_argument("No argument entered");
	}
	
	// Are we in debugging mode?
	bool DEBUG_MODE = true;
	
	// Get aiscene with assimp
	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(argv[1], aiProcess_Triangulate | aiProcess_GenNormals | aiProcess_JoinIdenticalVertices | aiProcess_CalcTangentSpace);
	
	// Check if import was successful
	if( (!scene) || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !(scene->mRootNode) ) {
		cout << "import was unsuccessful" << endl;
		return false;
	}
	
	// Vector of MeshGLs
	vector<MeshGL> meshgls;

	// GLFW setup
	GLFWwindow* window = setupGLFW(4, 3, 800, 800, DEBUG_MODE);

	// GLEW setup
	setupGLEW(window);

	//initial variables
	double mx, my;
	glfwGetCursorPos(window, &mx, &my);
	mousePos = glm::vec2(mx, my);

	//set the key callback function
	glfwSetKeyCallback(window, key_callback);

	//set the mouse callback function
	glfwSetCursorPosCallback(window, mouse_position_callback);

	//hides the cursor
	//glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

	// Check OpenGL version
	checkOpenGLVersion();

	// Set up debugging (if requested)
	if(DEBUG_MODE) checkAndSetupOpenGLDebugging();

	// Set the background color to a shade of blue
	glClearColor(0.0f, 216.0f, 255.0f, 1.0f);	

	// Create and load shader
	GLuint programID = 0;
	try {		
		// Load vertex shader code and fragment shader code
		string vertexCode = readFileToString("./Basic.vs");
		string fragCode = readFileToString("./Basic.fs");

		// Print out shader code, just to check
		if(DEBUG_MODE) printShaderCode(vertexCode, fragCode);

		// Create shader program from code
		programID = initShaderProgramFromSource(vertexCode, fragCode);
	}
	catch (exception e) {		
		// Close program
		cleanupGLFW(window);
		exit(EXIT_FAILURE);
	}
	
	// Setup shape
	for ( int i = 0; i < scene->mNumMeshes; i++ ) {
		Mesh m;
		MeshGL mgl;
		extractMeshData(scene->mMeshes[i], m);
		createMeshGL(m, mgl);
		meshgls.push_back(mgl);
	}

	// Get the matrix locations
	GLint modelMatLoc = glGetUniformLocation(programID, "modelMat");
	GLint viewMatLoc = glGetUniformLocation(programID, "viewMat");
    GLint projMatLoc = glGetUniformLocation(programID, "projMat");
	GLint normMatLoc = glGetUniformLocation(programID, "normMat");

	//Setup light
	light.pos =  glm::vec4(0.5, 0.5, 0.5, 1.0);
	light.color =  glm::vec4(1.0, 1.0, 1.0, 1.0);
	GLint lightPosLoc = glGetUniformLocation(programID, "light.pos");
    GLint lightColorLoc = glGetUniformLocation(programID, "light.color");

	//setup roughness and metallic
	GLint roughnessLoc = glGetUniformLocation(programID, "roughness");
	GLint metallicLoc = glGetUniformLocation(programID, "metallic");

	//setup texcoords and tangent
	GLint diffuseTextureLoc = glGetUniformLocation(programID, "diffuseTexture");
	GLint normalTextureLoc = glGetUniformLocation(programID, "normalTexture");
	unsigned int diffuseID = loadAndCreateTexture("4977210.jpg");
	unsigned int normalID = loadAndCreateTexture("./sampleModels/NormalMap.png");

	/*
	// Create simple quad
	Mesh m;
	createSimpleQuad(m);

	// Create OpenGL mesh (VAO) from data
	MeshGL mgl;
	createMeshGL(m, mgl);
	*/
	
	// Enable depth testing
	glEnable(GL_DEPTH_TEST);

	while (!glfwWindowShouldClose(window)) {
		// Set viewport size
		int fwidth, fheight;
		glfwGetFramebufferSize(window, &fwidth, &fheight);
		glViewport(0, 0, fwidth, fheight);

		// Clear the framebuffer
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Use shader program
		glUseProgram(programID);

		glfwGetFramebufferSize(window, &fwidth, &fheight);
		double aspectRatio;
		if(fwidth == 0 || fheight == 0){
			aspectRatio = 1.0;
		}
		else{
			aspectRatio = fwidth/fheight;
		}

		//Calculation of View Matrix
		glm::mat4 viewMat = glm::lookAt(eye, lookAt, glm::vec3(0,1,0));
		glUniformMatrix4fv(viewMatLoc, 1, false, glm::value_ptr(viewMat));

		//Calculation of Projection Matrix
		glm::mat4 projMat = glm::perspective(glm::radians(90.0), aspectRatio, 0.01, 50.0);
		glUniformMatrix4fv(projMatLoc, 1, false, glm::value_ptr(projMat));

		// Calculation of light
		glm::vec4 lightPos = viewMat * light.pos;
		glUniform4fv(lightPosLoc, 1, glm::value_ptr(light.pos));
		glUniform4fv(lightColorLoc, 1, glm::value_ptr(light.color));

		// Calculation using Roughness and metallic
		glUniform1f(roughnessLoc, roughness);
		glUniform1f(metallicLoc, metallic);

		// Calculation of Diffuse Texture and Tangents
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, diffuseID);
		glUniform1i(diffuseTextureLoc, 0);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, normalID);
		glUniform1i(normalTextureLoc, 1);
		
		/*
		// Draw objects
		for (MeshGL mgl : meshgls) {
			drawMesh(mgl);
		}
		*/

		renderScene(meshgls, scene->mRootNode, glm::mat4(1.0), modelMatLoc, normMatLoc, viewMat, 0);

		// Swap buffers and poll for window events		
		glfwSwapBuffers(window);
		glfwPollEvents();

		// Sleep for 15 ms
		this_thread::sleep_for(chrono::milliseconds(15));
	}

	// Clean up meshes
	for (MeshGL mgl : meshgls) {
		cleanupMesh(mgl);
	}

	// Clean up shader programs
	glUseProgram(0);
	glDeleteProgram(programID);
		
	// Destroy window and stop GLFW
	cleanupGLFW(window);

	return 0;
}
