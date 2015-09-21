#include "shader.h" // header file of shader loaders
#include "objectLoader.h" // header file of object loader
#include <GL/glew.h> // glew must be included before the main gl libs
#include <GL/glut.h> // doing otherwise causes compiler shouting
#include <iostream>
#include <fstream>
#include <chrono>
#include <string>
#include <cstring>
#include <vector>

#define GLM_FORCE_RADIANS

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp> // makes passing matrices to shaders easier

//--Data types
// This object defines the attributes of a vertex(position, color, etc...)
/*struct Vertex
{
    GLfloat position[3];
    GLfloat color[3];
};*/

//--Global constants
const char* vsFileName = "../bin/shader.vs";
const char* fsFileName = "../bin/shader.fs";

// Global variables

int w = 640, h = 480, geometrySize;// Window size
GLuint program;// The GLSL program handle
GLuint vbo_geometry;// VBO handle for our geometry
GLuint uvbuffer; // UV buffer
ShaderLoader programLoad; // Load shader class

    // Quit call
    bool quitCall = false;

// filename string
char * objPtr;

// uniform locations
GLint loc_mvpmat;// Location of the modelviewprojection matrix in the shader

// attribute locations
GLint loc_position;
GLint loc_color;

// transform matrices
glm::mat4 model;// obj-> world (planet) 
glm::mat4 view;// world->eye
glm::mat4 projection;// eye->clip
glm::mat4 mvp;// premultiplied modelviewprojection

//--GLUT Callbacks
void render();

  // update display functions
  void update();
  void reshape(int n_w, int n_h);

  // called upon input
  void keyboard(unsigned char key, int x_pos, int y_pos);
  void manageMenus();
  void menu(int id);
  void mouse(int button, int state, int x_pos, int y_pos);

//--Resource management
bool initialize();
void cleanUp();

//--Main
int main(int argc, char **argv)
{
    // If the user didn't provide a filename command line argument,
    // print an error and exit.
    if (argc <= 1)
    {
        std::cout << "ERROR: Usage: " << argv[0] << " <Filename>" << std::endl;
        exit(1);
    }

    // Initialize glut
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_DEPTH);
    glutInitWindowSize(w, h);

    // Get filename of object
    objPtr = argv[1];

    // Name and create the Window
    glutCreateWindow("Model Loader");

    // Now that the window is created the GL context is fully set up
    // Because of that we can now initialize GLEW to prepare work with shaders
    GLenum status = glewInit();
    if( status != GLEW_OK)
    {
        std::cerr << "[F] GLEW NOT INITIALIZED: ";
        std::cerr << glewGetErrorString(status) << std::endl;
        return -1;
    }

    // Set all of the callbacks to GLUT that we need
    glutDisplayFunc(render);// Called when its time to display
    glutReshapeFunc(reshape);// Called if the window is resized
    glutIdleFunc(update);// Called if there is nothing else to do
    glutKeyboardFunc(keyboard);// Called if there is keyboard input

    // add menus
    manageMenus();

    // Initialize all of our resources(shaders, geometry)
    bool init = initialize();
    if(init)
    {
        glutMainLoop();
    }

    // clean up and end program
    cleanUp();
    return 0;
}

//--Implementations
void render()
{
    //--Render the scene

    //clear the screen
    glClearColor(0.0, 0.0, 0.2, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    //enable the shader program
    glUseProgram(program);

    // render first object

      //premultiply the matrix for this example
      mvp = projection * view * model;

      //upload the matrix to the shader
      glUniformMatrix4fv(loc_mvpmat, 1, GL_FALSE, glm::value_ptr(mvp));

      //set up the Vertex Buffer Object so it can be drawn
      glEnableVertexAttribArray(loc_position);
      glEnableVertexAttribArray(loc_color);
      glBindBuffer(GL_ARRAY_BUFFER, vbo_geometry);
      glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);

        //set pointers into the vbo for each of the attributes(position and color)
        // 1rst attribute buffer : vertices
        glVertexAttribPointer( loc_position,//location of attribute
                             3,//number of elements
                             GL_FLOAT,//type
                             GL_FALSE,//normalized?
                             0,//stride
                             (void*)0;//offset

        // 2nd attribute buffer : UVs  
        glVertexAttribPointer( loc_color,
                             3,
                             GL_FLOAT,
                             GL_FALSE,
                             0,
                             (void*)0;

    glDrawArrays(GL_TRIANGLES, 0, geometrySize);//mode, starting index, count

    // done rendering objects

    //clean up
    glDisableVertexAttribArray(loc_position);
    glDisableVertexAttribArray(loc_color);
                           
    //swap the buffers
    glutSwapBuffers();

}

// called on idle to update display
void update()
{
    // check for quit program
    if( quitCall )
    {
      manageMenus();
      exit(0);
    }
    // otherwise, continue display
    else 
    {
      // update the state of the scene
      glutPostRedisplay();//call the display callback
    }
}

// resize window
void reshape(int n_w, int n_h)
{
    w = n_w;
    h = n_h;
    //Change the viewport to be correct
    glViewport( 0, 0, w, h);
    //Update the projection matrix as well
    //See the init function for an explaination
    projection = glm::perspective(45.0f, float(w)/float(h), 0.01f, 100.0f);

}

// called on keyboard input
void keyboard(unsigned char key, int x_pos, int y_pos )
{
    // Handle keyboard input
    // end program
    if(key == 27) // esc
    {
      quitCall = true;
      glutIdleFunc(update);
      exit(0);
    }
    // continue program
  // redraw screen 
  glutPostRedisplay();    
}

// initialize basic geometry and shaders for this example
bool initialize()
{
    // define model with model loader
    bool result;

    //enable depth testing
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    // loads shaders to program
    programLoad.loadShader( vsFileName, fsFileName,  program );

    // now we set the locations of the attributes and uniforms
    // this allows us to access them easily while rendering
    loc_position = glGetAttribLocation(program, const_cast<const char*>("v_position"));
    if(loc_position == -1)
    {
        std::cerr << "[F] POSITION NOT FOUND" << std::endl;
        return false;
    }

    loc_color = glGetAttribLocation(program, const_cast<const char*>("v_color"));
    if(loc_color == -1)
    {
        std::cerr << "[F] V_COLOR NOT FOUND" << std::endl;
        return false;
    }

    loc_mvpmat = glGetUniformLocation(program, const_cast<const char*>("mvpMatrix"));
    if(loc_mvpmat == -1)
    {
        std::cerr << "[F] MVPMATRIX NOT FOUND" << std::endl;
        return false;
    }

      // Read our .obj file
      std::vector<glm::vec3> geometry;
      std::vector<glm::vec2> uvs;
      std::vector<glm::vec3> normals; // Won't be used at the moment.
      result = loadOBJ( objPtr, geometry, uvs, normals);
      geometrySize = geometry.size();

      if( !result )
      {
        quitCall = true;
        glutIdleFunc(update);
        exit(0);
      }

    // create a Vertex Buffer object to store this vertex info on the GPU
    glGenBuffers(1, &vbo_geometry);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_geometry);
    glBufferData(GL_ARRAY_BUFFER, geometry.size() * sizeof(glm::vec3), &geometry[0], GL_STATIC_DRAW);

    glGenBuffers(1, &uvbuffer);
    glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
    glBufferData(GL_ARRAY_BUFFER, uvs.size() * sizeof(glm::vec2), &uvs[0], GL_STATIC_DRAW);
    
    //--Init the view and projection matrices
    //  if you will be having a moving camera the view matrix will need to more dynamic
    //  ...Like you should update it before you render more dynamic 
    //  for this project having them static will be fine
    view = glm::lookAt( glm::vec3(0.0, 8.0, -16.0), //Eye Position
                        glm::vec3(0.0, 0.0, 0.0), //Focus point
                        glm::vec3(0.0, 1.0, 0.0)); //Positive Y is up

    projection = glm::perspective( 45.0f, //the FoV typically 90 degrees is good which is what this is set to
                                   float(w)/float(h), //Aspect Ratio, so Circles stay Circular
                                   0.01f, //Distance to the near plane, normally a small value like this
                                   100.0f); //Distance to the far plane, 

    //and its done
    return true;
}

// delete old items
void cleanUp()
{
    // Clean up, Clean up
    glDeleteProgram(program);
    glDeleteBuffers(1, &vbo_geometry);
    glDeleteBuffers(1, &uvbuffer);    
}

// adds and removes menus
void manageMenus()
{
  int main_menu = 0;

  // upon initialization
  if( !quitCall )
  {
    // create main menu
    main_menu = glutCreateMenu(menu); // Call menu function
    glutAddMenuEntry("Quit", 1);
    glutAttachMenu(GLUT_RIGHT_BUTTON); //Called if there is a mouse click (right)
  }
  // destroy menus before ending program
  else
  {
    // Clean up after ourselves
    glutDestroyMenu(main_menu);
  }

  // update display
  glutPostRedisplay();
}

// menu choices 
void menu(int id)
{
  // switch case for menu options
  switch(id)
  {
    // call the rotation menu function
    case 1:
      quitCall = true;
      glutIdleFunc(update);
      break;
    default:
      break;
  }
  // redraw screen without menu
  glutPostRedisplay();
}

// actions for left mouse click
void mouse(int button, int state, int x_pos, int y_pos)
{
  // redraw screen without menu
  glutPostRedisplay();
}
