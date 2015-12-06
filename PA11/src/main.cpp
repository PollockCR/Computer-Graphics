// removed idle function, not in use
// created seperate files for fragment and vertex shader
#include "shader.h" // header file of shader loaders
#include "mesh.h" // header file of object loader
#include "loadImage.h" // header file for image class
#include <GL/glew.h> // glew must be included before the main gl libs
#include <GL/freeglut.h>
#include <iostream>
#include <fstream>
#include <chrono>
#include <string>
#include <cstring>
#include <vector>

// Assimp
#include <assimp/Importer.hpp> // C++ importer interface
#include <assimp/scene.h> // Output data structure
#include <assimp/postprocess.h> // Post processing flags
#include <assimp/color4.h> // Post processing flags

// GLM
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp> // makes passing matrices to shaders easier

// MAGICK++
#include <Magick++.h>

// Bullet library
#include <btBulletDynamicsCommon.h>

// DATA TYPES

// GLOBAL CONSTANTS
const char* vsFileName = "../bin/shader.vs";
const char* fsFileName1 = "../bin/shader1.fs";
const char* fsFileName2 = "../bin/shader2.fs";
const char* fsFileName3 = "../bin/shader3.fs";
const char* fsFileName4 = "../bin/shader4.fs";
const char* defaultInfo = "../bin/imageInfo.txt";
const char* blankTexture = "../../Resources/white.png";

// GLOBAL VARIABLES

  // Window size
  int w = 1280, h = 768;

  // geomerty size
  int geometrySize;

  // The GLSL program handle
  GLuint program;
  //GLuint vbo_geometry;
  //GLuint normalbuffer; // Normal Buffer
  //GLuint texture;

  // rotations
  int orbit = -1;
  int rotation = -1;

  // uniform locations
  GLint loc_mvpmat;// Location of the modelviewprojection matrix in the shader
  GLint viewMatrixID;
  GLint modelMatrixID;

  // attribute locations
  GLint loc_position;
  GLint loc_texture;
  GLint vertexNormal_modelspaceID;

  // transform matrices
  glm::mat4 view;// world->eye
  glm::mat4 projection;// eye->clip
  glm::mat4 mvp;// premultiplied modelviewprojection

  // Images
  std::vector<Image> images;
  int numImages = 0;
  int viewType = 1;

  // time information
  std::chrono::time_point<std::chrono::high_resolution_clock> t1, t2;

// FUNCTION PROTOTYPES

  //--GLUT Callbacks
  void render();

  // update display functions
  void update();
  void reshape(int n_w, int n_h);

  // called upon input
  void keyboard(unsigned char key, int x_pos, int y_pos);
  void manageMenus(bool quitCall);
  void menu(int id);
  void Menu1(int num);
  void Menu2(int num);
  void mouse(int button, int state, int x_pos, int y_pos);
  void ArrowKeys(int button, int x_pos, int y_pos);

  //--Resource management
  bool initialize( const char* filename);
  void cleanUp();

  //--Time function
  float getDT();

  //Load Image info
  bool loadInfo( const char* infoFilepath, std::vector<Mesh> &meshes, int numOfImages );

  void Sprint( float x, float y, const char *st);

  //Bullet 
  btDiscreteDynamicsWorld *dynamicsWorld;
  btRigidBody *rigidBodySphere;
  btRigidBody *rigidBodyCube;
  btRigidBody *rigidBodyCylinder;
  btTriangleMesh *trimesh = new btTriangleMesh();
  
  //Bullet collisions
  #define BIT(x) (1<<(x))
  enum collisionObject 
  {
    ball = BIT(0), 
    wall = BIT(1), 
    win = BIT(2), 
  };

  int ballBouncesOff = wall | win;
  int wallDeflects = ball;
  
  //pan
  void pan();
  

  //directions
  bool forward = false;
  bool backward = false;
  bool goLeft = false;
  bool goRight = false;
  double rotationDegrees = 0.0;
  bool cylforward = false;
  bool cylbackward = false;
  bool cylgoLeft = false;
  bool cylgoRight = false;
  
  //current view position
  static float* source = new float[3];
  static float* dest = new float[3];
  
  // Update view flag
  bool updateViewFlag = false;


// MAIN FUNCTION
int main(int argc, char **argv)
{
    bool init = false;

    // Initialize glut
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_DEPTH);
    glutInitWindowSize(w, h);
    
    // Name and create the Window
    glutCreateWindow("Dope Lighting Demo");

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
    glutMouseFunc(mouse);//Called if there is mouse input
    glutSpecialFunc(ArrowKeys);
	int index = glutCreateMenu(Menu1);
	glutAddMenuEntry("Rotate Clockwise", 1);
	glutAddMenuEntry("Rotate Counterclockwise", 2);
	glutAddMenuEntry("Don't Rotate", 3);
	glutCreateMenu(Menu2);
	glutAddSubMenu("Rotation options", index);
	glutAddMenuEntry("Exit Program", 2);
	glutAttachMenu(GLUT_RIGHT_BUTTON);
	srand(getDT());

    // add menus
    manageMenus( false );
    
    // Initialize all of our resources(shaders, geometry)
    // pass default planet info if not given one 
    if( argc != 3 )
    {
      init = initialize( defaultInfo );
    }
    // or, pass planet info given from command line argument
    else
    {
      init = initialize( argv[1] );
    }

//////////////////////////////////////////////////////////////////////////
    //create brodphase
    btBroadphaseInterface* broadphase = new btDbvtBroadphase();

    //create collision configuration
    btDefaultCollisionConfiguration* collisionConfiguration = new btDefaultCollisionConfiguration();

    //create a dispatcher
    btCollisionDispatcher* dispatcher = new btCollisionDispatcher(collisionConfiguration);

    //create a solver
    btSequentialImpulseConstraintSolver* solver = new btSequentialImpulseConstraintSolver;

    //create the physics world
    dynamicsWorld = new btDiscreteDynamicsWorld(dispatcher, broadphase, solver, collisionConfiguration);

     //set the gravity
     dynamicsWorld->setGravity(btVector3(0, -10, 0));

    /*//create a game board which the objects will be on
    btCollisionShape* ground = new btStaticPlaneShape(btVector3(0, 1, 0), 1);
    btCollisionShape* wallOne = new btStaticPlaneShape(btVector3(-1, 0, 0), 1);
    btCollisionShape* wallTwo = new btStaticPlaneShape(btVector3(1, 0, 0), 1);
    btCollisionShape* wallThree = new btStaticPlaneShape(btVector3(0, 0, 1), 1);
    btCollisionShape* wallFour = new btStaticPlaneShape(btVector3(0, 0, -1), 1);*/
    
    // now we can make the maze in one line
     btBvhTriangleMeshShape* mazeShape = new btBvhTriangleMeshShape(trimesh, false);

    //create sphere and set radius to 1
    btCollisionShape* sphere = new btSphereShape(.3);


/*----------------------this is the gameboard--------------------------------*/        
  // After we create collision shapes we have to se the default motion state 
    // for the ground
    btDefaultMotionState* groundMotionState = NULL;
    groundMotionState = new btDefaultMotionState(btTransform(btQuaternion(0, 0, 0, 1), btVector3(0, 0, 0)));
    //here we construct the ground using the motion state and shape
    btRigidBody::btRigidBodyConstructionInfo groundRigidBodyCI(0, groundMotionState, mazeShape, btVector3(0, 0, 0));
    btRigidBody* groundRigidBody = new btRigidBody(groundRigidBodyCI);

    //display dynamic body in our world
    dynamicsWorld->addRigidBody(groundRigidBody, wall, wallDeflects );
/*-----------------------------------------------------------------------------*/


/*----------------------this is the sphere--------------------------------*/        
  // After we create collision shapes we have to se the default motion state 
    // for the sphere
    btDefaultMotionState* sphereMotionState = NULL;
    sphereMotionState = new btDefaultMotionState(btTransform(btQuaternion(0, 0, 0, 1), btVector3(2, 1, 0)));

    // the sphere must have a mass
    btScalar mass = 50;

    //we need the inertia of the sphere and we need to calculate it
    btVector3 sphereInertia(0, 0, 0);
    sphere->calculateLocalInertia(mass, sphereInertia);

    //Here we construct the sphere with a mass, motion state, and inertia
    btRigidBody::btRigidBodyConstructionInfo sphereRigidBodyCI(mass, sphereMotionState, sphere, sphereInertia);
    rigidBodySphere = new btRigidBody(sphereRigidBodyCI);
    rigidBodySphere->setActivationState(DISABLE_DEACTIVATION);
    rigidBodySphere->setFriction(50);
    rigidBodySphere->setRestitution(0);

    //display dynamic body in our world
    dynamicsWorld->addRigidBody(rigidBodySphere, ball, ballBouncesOff);
/*-----------------------------------------------------------------------------*/
        



    // if initialized, begin glut main loop
    if(init)
    {
        t1 = std::chrono::high_resolution_clock::now();
        glutMainLoop();
    }

    // remove menus
    manageMenus( true );

    //////////// clean up and end program
    // delete the pointers
    dynamicsWorld->removeRigidBody(groundRigidBody);
    delete groundRigidBody->getMotionState();
    delete groundRigidBody;
        
    dynamicsWorld->removeRigidBody(rigidBodySphere);
    delete rigidBodySphere->getMotionState();
    delete rigidBodySphere;
    delete sphere;
    delete dynamicsWorld;
    delete solver;
    delete collisionConfiguration;
    delete dispatcher;
    delete broadphase;

    cleanUp();
    return 0;
}

// FUNCTION IMPLEMENTATION

//the first menu that appears
void Menu1(int num)
    {
	switch(num)
		{
		case 1: 
			rotationDegrees = 45.0;
			break;
		case 2: 
			rotationDegrees =-45.0;
			break;
		case 3: 
			rotationDegrees = 0.0;
			break;
		}
	glutPostRedisplay();
	}

//the second sub-menu
void Menu2(int num)
    {
	switch (num)
		{
		case 1:
			Menu1(num);
			break;
		case 2:
			exit(0);
			break;
		}
	glutPostRedisplay();
	}


// render the scene
void render()
{
  // clear the screen
  glClearColor(0.0, 0.0, 0.2, 1.0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(0);
    char* Text = new char[100];
    std::string viewText;

    
    Text = (char*) "WASD to move sphere, Arrow keys to move cylinder, 1-4 to change light";
    Sprint(-0.9,0.9,Text);

    if( viewType == 1 )
    {
      viewText = "Light source: Ambient";
    }
    else if( viewType == 2 )
    {
      viewText = "Light source: Spotlight";
    }
    else if( viewType == 3 )
    {
      viewText = "Light source: Point";
    }
    else if( viewType == 4 )
    {
      viewText = "Light source: Distance";
    }    

    Sprint(-0.9,0.8,viewText.c_str());

  // enable the shader program
  glUseProgram(program);
  GLuint lightID = glGetUniformLocation(program, "LightPosition_worldspace");


    if( viewType == 2 || viewType == 3 )
    {
      // light
      glm::vec3 lightPos = glm::vec3(0,4,0);
      glUniform3f(lightID, lightPos.x, lightPos.y, lightPos.z);
    }
    else if( viewType == 4 )
    {
      glm::vec3 lightPos = glm::vec3(0,1,-5);
      glUniform3f(lightID, lightPos.x, lightPos.y, lightPos.z);
    }

 // loop through each planet
    for( int index = 0; index < numImages; index++ )
    {
      // premultiply the matrix for this example
      images[index].mvp = projection * view * images[index].model;

      // upload the matrix to the shader
      glUniformMatrix4fv(loc_mvpmat, 1, GL_FALSE, &(images[index].mvp[0][0])); 
      glUniformMatrix4fv(modelMatrixID, 1, GL_FALSE, &(images[index].model[0][0]));
      glUniformMatrix4fv(viewMatrixID, 1, GL_FALSE, &(images[index].view[0][0]));    

      // set up the Vertex Buffer Object so it can be drawn
      glEnableVertexAttribArray(loc_position);
      glBindBuffer(GL_ARRAY_BUFFER, images[index].vbo_geometry);

      // set pointers into the vbo for each of the attributes(position and color)
      glVertexAttribPointer( loc_position,//location of attribute
                             3,//number of elements
                             GL_FLOAT,//type
                             GL_FALSE,//normalized?
                             sizeof(Vertex),//stride
                             0);//offset

      glEnableVertexAttribArray(loc_texture);
      glBindTexture(GL_TEXTURE_2D, images[index].texture);

      glVertexAttribPointer( loc_texture,
                             2,
                             GL_FLOAT,
                             GL_FALSE,
                             sizeof(Vertex),
                             (void*)offsetof(Vertex,uv));


      glEnableVertexAttribArray(vertexNormal_modelspaceID);
      glBindBuffer(GL_ARRAY_BUFFER, images[index].normalbuffer);

        // 3rd attribute buffer : normals  
        glVertexAttribPointer( vertexNormal_modelspaceID,
                               3,
                               GL_FLOAT,
                               GL_FALSE,
                               sizeof(Vertex),
                               (void*)offsetof(Vertex,normals));       

      glDrawArrays(GL_TRIANGLES, 0, images[index].geometrySize);//mode, starting index, count
    }

  //clean up
  glDisableVertexAttribArray(loc_position);
  glDisableVertexAttribArray(loc_texture);
  glDisableVertexAttribArray(vertexNormal_modelspaceID);   
               
  //swap the buffers
  glutSwapBuffers();

}

// called on idle to update display
void update()
{
  // update object
    float dt = getDT();
    float force = 10.0;
    
    // add the forces to the sphere for movement
    if(forward)
    {
        rigidBodySphere->applyCentralImpulse(btVector3(0.0,0.0,force));
        forward = false;
    }
    if(backward)
    {
        rigidBodySphere->applyCentralImpulse(btVector3(0.0,0.0,-force));
        backward = false;
    }
    if(goLeft)
    {
        rigidBodySphere->applyCentralImpulse(btVector3(force,0.0,0.0));
        goLeft = false;
    }
    if(goRight)
    {
        rigidBodySphere->applyCentralImpulse(btVector3(-force,0.0,0.0));
        goRight = false;
    }

    
    dynamicsWorld->stepSimulation(dt, 10);


    btTransform trans;

    btScalar m[16];

    //set the sphere to it's respective model
    rigidBodySphere->getMotionState()->getWorldTransform(trans);
    trans.getOpenGLMatrix(m);
    images[1].model = glm::make_mat4(m);

   

  // update the state of the scene
  glutPostRedisplay();//call the display callback

  // clean up!
  rigidBodySphere->clearForces();
}

// resize window
void reshape(int n_w, int n_h)
{
    // set new window width and height
    w = n_w;
    h = n_h;

    // change the viewport to be correct
    glViewport( 0, 0, w, h);

    // update the projection matrix
    projection = glm::perspective(45.0f, float(w)/float(h), 0.01f, 100.0f);

}

// called on keyboard input
void keyboard(unsigned char key, int x_pos, int y_pos )
{
        ShaderLoader programLoad;
  // Handle keyboard input - end program
    if((key == 27)||(key == 'q')||(key == 'Q'))
    {
      glutLeaveMainLoop();
    }
    else if((key == 'w')||(key == 'W'))
    {
      forward = true;
    }
    else if((key == 'a')||(key == 'A'))
    {
      goLeft = true;
    }
    else if((key == 's')||(key == 'S'))
    {
      backward = true;
    }
    else if((key == 'd')||(key == 'D'))
    {
      goRight = true;
    }
    else if(key == '1')
    {
      programLoad.loadShader( vsFileName, fsFileName1, program );
      viewType = 1;
    }         
    else if(key == '2')
    {
      programLoad.loadShader( vsFileName, fsFileName2, program );
      viewType = 2;
    }  
    else if(key == '3')
    {
      programLoad.loadShader( vsFileName, fsFileName3, program );
      viewType = 3;
    }  
    else if(key == '4')
    {
      programLoad.loadShader( vsFileName, fsFileName4, program );
      viewType = 4;
    }   
    
    
    // pan
    if((key == 'i')||(key == 'I'))
    {
      dest[0] = 0.0;
      dest[1] = 18.0;
      dest[2] = 18.0;
      updateViewFlag = true;        
      pan();
    }
    if((key == 'j')||(key == 'J'))
    {
      dest[0] = 18.0;
      dest[1] = 18.0;
      dest[2] = 0.0; 
      updateViewFlag = true;        
      pan();
    }
    if((key == 'k')||(key == 'K'))
    {
      dest[0] = 0.0;
      dest[1] = 18.0;
      dest[2] = -18.0; 
      updateViewFlag = true;        
      pan();
    }
    if((key == 'l')||(key == 'L'))
    {
      dest[0] = -18.0;
      dest[1] = 18.0;
      dest[2] = 0.0; 
      updateViewFlag = true;        
      pan(); 
    }   
}

// initialize basic geometry and shaders for this example
bool initialize( const char* filename)
{
    // define model with model loader
    /*bool geometryLoadedCorrectly;
    Mesh object;
    ShaderLoader programLoad;*/
    int index;
    std::vector<Mesh> meshes;
    ShaderLoader programLoad;

    // load the image info
    if( !loadInfo( filename, meshes, numImages ) )
    {
      return false;
    }


     // loop through each planet
      for( index = 0; index < numImages; index++ )
      {
        // Create a Vertex Buffer object to store this vertex info on the GPU
        glGenBuffers(1, &(images[index].vbo_geometry));
        glBindBuffer(GL_ARRAY_BUFFER, images[index].vbo_geometry);
        glBufferData(GL_ARRAY_BUFFER, meshes[index].geometry.size()* sizeof(Vertex), &(meshes[index].geometry[0]), GL_STATIC_DRAW);

        // Create Texture object
        glGenTextures(1, &(images[index].texture));
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, images[index].texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, images[index].imageCols, images[index].imageRows, 0, GL_RGBA, GL_UNSIGNED_BYTE, images[index].m_blob.data());
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);      

        glGenBuffers(1, &(images[index].normalbuffer));
        glBindBuffer(GL_ARRAY_BUFFER, images[index].normalbuffer);
        glBufferData(GL_ARRAY_BUFFER, meshes[index].geometry.size() * sizeof(Vertex), &(meshes[index].geometry[0]), GL_STATIC_DRAW);    
        //glBufferData(GL_ARRAY_BUFFER, meshes[index].geometry.size() * sizeof(Vertex), &(meshes[index].geometry[offsetof(Vertex,normals)]), GL_STATIC_DRAW);    

      }      

      //if( viewType == 0 )
      //{
        // loads shaders to program
        programLoad.loadShader( vsFileName, fsFileName1, program );
      //}
      /*
      else
      {
        // loads shaders to program
        programLoad.loadShader( vsFileName, fsFileName1, program );        
      }
*/
    // Get a handle for our "MVP" uniform
    loc_mvpmat = glGetUniformLocation(program, "mvpMatrix");
      if(loc_mvpmat == -1)
      {
        std::cerr << "[F] MVP MATRIX NOT FOUND" << std::endl;
        return false;
      }       

    viewMatrixID = glGetUniformLocation(program, "V");
      if(viewMatrixID == -1)
      {
        std::cerr << "[F] VIEW NOT FOUND" << std::endl;
        return false;
      }  

    modelMatrixID = glGetUniformLocation(program, "M");
      if(modelMatrixID == -1)
      {
        std::cerr << "[F] MODEL NOT FOUND" << std::endl;
        return false;
      }    
/*
    // Get a handle for our buffers
    loc_position = glGetAttribLocation(program, "v_position");
      if(loc_position == -1)
      {
        std::cerr << "[F] POSITION NOT FOUND" << std::endl;
        return false;
      }

    loc_texture = glGetAttribLocation(program, "v_color");
      if(loc_texture == -1)
      {
        std::cerr << "[F] COLOR NOT FOUND" << std::endl;
        return false;
      }    
*/  

    // Get a handle for our buffers
    loc_position = glGetAttribLocation(program, "v_position");
      if(loc_position == -1)
      {
        std::cerr << "[F] POSITION NOT FOUND" << std::endl;
        return false;
      }
    
    loc_texture = glGetAttribLocation(program, "v_color");
      if(loc_texture == -1)
      {
        std::cerr << "[F] UV NOT FOUND" << std::endl;
        return false;
      }

    vertexNormal_modelspaceID = glGetAttribLocation(program, "vertexNormal_modelspace");
      if(vertexNormal_modelspaceID == -1)
      {
        std::cerr << "[F] NORMAL NOT FOUND" << std::endl;
        return false;
      }


    // set view variables
    source[0] = 0.0;
    source[1] = 18.0;
    source[2] = -18.0;  
    dest[0] = 0.0;
    dest[1] = 18.0;
    dest[2] = -18.0;


    //--Init the view and projection matrices
    //  if you will be having a moving camera the view matrix will need to more dynamic
    //  ...Like you should update it before you render more dynamic 
    //  for this project having them static will be fine
    view = glm::lookAt( glm::vec3(source[0], source[1], source[2]), //Eye Position
                        glm::vec3(0.0, 0.0, 0.0), //Focus point
                        glm::vec3(0.0, 1.0, 0.0)); //Positive Y is up

    projection = glm::perspective( 45.0f, //the FoV typically 90 degrees is good which is what this is set to
                                   float(w)/float(h), //Aspect Ratio, so Circles stay Circular
                                   0.01f, //Distance to the near plane, normally a small value like this
                                   100.0f); //Distance to the far plane
                                   
    
    images[numImages-1].model = glm::scale(images[numImages-1].model, glm::vec3(25, 25, 25));

    //enable depth testing
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    //and its done
    return true;
}

// load info from file into Images
bool loadInfo( const char* infoFilepath, std::vector<Mesh> &meshes, int numOfImages )
{
  // initialize variables
  std::ifstream ifs(infoFilepath, std::ifstream::in);
  bool geometryLoadedCorrectly = false;
  bool imageLoadedCorrectly = false;
  bool trimeshLoadedCorrectly = false;
  int index = 0;
  int numOfMeshes = 0;

  // check for open file
  if( !ifs.is_open() )
  {
    std::cerr << "[F] FAILED TO READ FILE!" << infoFilepath << std::endl;
    return false;    
  }

  // read in number of Images
  ifs >> numOfImages;

  // check for invalid file format
  if( numOfImages == -1 )
  {
    std::cerr << "[F] FAILED TO READ FILE! INVALID FORMAT" << std::endl;
    return false; 
  }

  // loop through each image
  for( index = 0; index < numOfImages; index++ )
  {
    // create new Image
    Image tempImage;
    images.push_back(tempImage);

    // create new mesh
    Mesh tempMesh;
    meshes.push_back(tempMesh);

    // read from file

      // load obj file
      std::string objFilepath;
      ifs >> objFilepath;
      geometryLoadedCorrectly = meshes[index].loadMesh( objFilepath.c_str(), numOfMeshes);

        // return false if not loaded
        if( !geometryLoadedCorrectly )
        {
          std::cerr << "[F] GEOMETRY NOT LOADED CORRECTLY" << std::endl;
          return false;
        }
        
       // set number of vertices
       unsigned int numOfVertices = meshes[index].geometry.size(); 
        std::cerr << numOfMeshes << std::endl;
       //check if we are reading in the maze 
       if (index == 0)
        {
            //load trimesh for maze
            for ( int index2 = 0; index2 < numOfMeshes; index2++)
            {
                for (unsigned int index3 = 0; index3 < numOfVertices; index3 += 3)
                {
                trimesh->addTriangle(btVector3(meshes[index2].geometry[index3].position[0], 
                                               meshes[index2].geometry[index3].position[1], 
                                               meshes[index2].geometry[index3].position[2]), 
                                           
                                     btVector3(meshes[index2].geometry[index3+1].position[0], 
                                               meshes[index2].geometry[index3+1].position[1], 
                                               meshes[index2].geometry[index3+1].position[2]), 
                                            
                                     btVector3(meshes[index2].geometry[index3+2].position[0], 
                                               meshes[index2].geometry[index3+2].position[1], 
                                               meshes[index2].geometry[index3+2].position[2]), false);  
                }
            }
        }

      // load texture
      std::string textureFilepath;
      ifs >> textureFilepath;
      imageLoadedCorrectly = images[index].loadImage(textureFilepath.c_str());

        // return false if not loaded
        if( !imageLoadedCorrectly )
        {
          return false;
        }

      // save size of geometry
      images[index].geometrySize = meshes[index].geometry.size();
    
  }

  // update image count
  numImages = images.size();       


  // return success
  return true;
}

// delete old items
void cleanUp()
{
 // initialize variables
  int index;

  // clean up programs
  glDeleteProgram(program);   

    // clean up each planet
    for( index = 0; index < numImages; index++ )
    {
      glDeleteBuffers(1, &(images[index].vbo_geometry));
      glDeleteBuffers(1, &(images[index].texture));
      glDeleteBuffers(1, &(images[index].normalbuffer));        
    }

}

// adds and removes menus
void manageMenus( bool quitCall )
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
    // clean up after ourselves
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
      glutLeaveMainLoop();
      break;
      
    case 2: // Player 1 POV 
      dest[0] = 0.0;
      dest[1] = 18.0;
      dest[2] = -18.0;
      source[0] = 0.0;
      source[1] = 18.0;
      source[2] = -18.0; 
      updateViewFlag = true;   
      pan();
      break;
    case 3: // Player 2 POV

      dest[0] = 0.0;
      dest[1] = 18.0;
      dest[2] = 18.0;
      source[0] = 0.0;
      source[1] = 18.0;
      source[2] = 18.0;   
      updateViewFlag = true;    
      pan();
      break;
    case 4: // Left side POV
  
      dest[0] = 18.0;
      dest[1] = 18.0;
      dest[2] = 0.0;
      source[0] = 18.0;
      source[1] = 18.0;
      source[2] = 0.0;  
      updateViewFlag = true;     
      pan();
      break;
    case 5: // Right side POV
      
      dest[0] = -18.0;
      dest[1] = 18.0;
      dest[2] = 0.0;
      source[0] = -18.0;
      source[1] = 18.0;
      source[2] = 0.0;     
      updateViewFlag = true;    
      pan();
      break;      

    // default do nothing
    default:
      break;
  }
  // redraw screen without menu
  glutPostRedisplay();
}

// actions for left mouse click
void mouse(int button, int state, int x_pos, int y_pos)
{
  if(button == GLUT_LEFT_BUTTON && state == GLUT_DOWN);
	else if (button == GLUT_RIGHT_BUTTON && state == GLUT_DOWN);
}

//returns the time delta
float getDT()
{
    float ret;

    // update time using time elapsed since last call
    t2 = std::chrono::high_resolution_clock::now();
    ret = std::chrono::duration_cast< std::chrono::duration<float> >(t2-t1).count();
    t1 = std::chrono::high_resolution_clock::now();
    return ret;
}

void ArrowKeys(int button, int x_pos, int y_pos)
{
    if (button == GLUT_KEY_LEFT)
    {
        cylgoLeft = true;
    }

    if (button == GLUT_KEY_RIGHT)
    {
        cylgoRight = true;
    }

    if (button == GLUT_KEY_UP)
    {
        cylforward = true;
    }

    if (button == GLUT_KEY_DOWN)
    {
        cylbackward = true;
    }
}

// This prints a string to the screen
void Sprint( float x, float y, const char *st)
{
    int l,i;

    l=strlen( st ); // see how many characters are in text string.
    glRasterPos2f(x, y); // location to start printing text
    for( i=0; i < l; i++) // loop until i is greater then l
    {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, st[i]); // Print a character on the screen
    }
}


void pan()
{
    // pre pan movement
      // make sure we want to move
      if (updateViewFlag)
      {


/*
        if ( (dest[0] == 0.0) && (dest[1] == 18.0) && (dest[2] == -18.0) )
            {
            player1POV = true;
            player2POV = false;  
            leftSidePOV = false; 
            rightSidePOV = false; 
            }

        if ( (dest[0] == 0.0) && (dest[1] == 18.0) && (dest[2] == 18.0) )
            {
            player1POV = false;
            player2POV = true;  
            leftSidePOV = false; 
            rightSidePOV = false; 
            }

        if ( (dest[0] == 18.0) && (dest[1] == 18.0) && (dest[2] == 0.0) )
            {
            player1POV = false;
            player2POV = false;  
            leftSidePOV = true; 
            rightSidePOV = false; 
            }

        if ( (dest[0] == -18.0) && (dest[1] == 18.0) && (dest[2] == 0.0) )
            {
            player1POV = false;
            player2POV = false;  
            leftSidePOV = false; 
            rightSidePOV = true; 
            }
*/


        //paused = true;
        // check source vs dest coords
        // increment accordingly
        for (int i = 0; i < 3; i++)
        {

            if (i < 3)
            {
              if (source[i] < dest[i]) 
                  source[i] += 0.18;
              if (source[i] > dest[i]) 
                  source[i] -= 0.18; 
                glutPostRedisplay(); 
            }
            else
            {
              if (source[i] < dest[i]) 
                  source[i] += 0.18;
              if (source[i] > dest[i]) 
                  source[i] -= 0.18; 
                glutPostRedisplay(); 
            }
        }
          
        // check acceptable ranges
        if (((dest[0] - source[0] <= 0.5) && (dest[0] - source[0] >= -0.5))&&
            ((dest[1] - source[1] <= 0.5) && (dest[1] - source[1] >= -0.5))&&
            ((dest[2] - source[2] <= 0.5) && (dest[2] - source[2] >= -0.5)))
        {
          // done with pan
          updateViewFlag = false;

          // update view to dest
          view = glm::lookAt( glm::vec3(dest[0], dest[1], dest[2]), //Eye Position
                  glm::vec3(0.0, 0.0, 0.0), //Focus point
                  glm::vec3(0.0, 1.0, 0.0)); //Positive Y is up

        }
        else
        {
          // update to incremented view
          view = glm::lookAt( glm::vec3(source[0], source[1], source[2]), //Eye Position
                glm::vec3(0.0, 0.0, 0.0), //Focus point
                glm::vec3(0.0, 1.0, 0.0)); //Positive Y is up
          glutPostRedisplay();  
        } 
      }
      
      else
      {
        // keep source data current
        for (int i = 0; i < 3; i++)
        {
            if (source[i] < dest[i]) 
                source[i] += 0.35;
            if (source[i] > dest[i]) 
                source[i] -= 0.35;
        }
        // locked view
        view = glm::lookAt( glm::vec3(dest[0], dest[1], dest[2]), //Eye Position
                glm::vec3(0.0, 0.0, 0.0), //Focus point
                glm::vec3(0.0, 1.0, 0.0)); //Positive Y is up
      }
    //}
}







