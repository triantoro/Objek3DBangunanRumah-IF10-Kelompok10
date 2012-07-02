#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifdef __APPLE__
#include <OpenGL/OpenGL.h>
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#include <GL/glu.h>
#include <GL/gl.h>
#include "imageloader.h"
#include "vec3f.h"
#endif



static GLfloat spin, spin2 = 0.0;
float angle = 0;
using namespace std;

float lastx, lasty;
GLint stencilBits;
static int viewx = 0;
static int viewy = 40; 
static int viewz =100;

float rot = 0;

GLuint texture[1]; //array untuk texture

struct Images {//tempat image
	unsigned long sizeX;
	unsigned long sizeY;
	char *data;
};
typedef struct Images Images;

//class untuk terain 2D
class Terrain {
private:
	int w; //Width
	int l; //Length
	float** hs; //Heights
	Vec3f** normals;
	bool computedNormals; //Whether normals is up-to-date
public:
	Terrain(int w2, int l2) {
		w = w2;
		l = l2;

		hs = new float*[l];
		for (int i = 0; i < l; i++) {
			hs[i] = new float[w];
		}

		normals = new Vec3f*[l];
		for (int i = 0; i < l; i++) {
			normals[i] = new Vec3f[w];
		}

		computedNormals = false;
	}

	Terrain() {
		for (int i = 0; i < l; i++) {
			delete[] hs[i];
		}
		delete[] hs;

		for (int i = 0; i < l; i++) {
			delete[] normals[i];
		}
		delete[] normals;
	}

	int width() {
		return w;
	}

	int length() {
		return l;
	}

	//Sets the height at (x, z) to y
	void setHeight(int x, int z, float y) {
		hs[z][x] = y;
		computedNormals = false;
	}

	//Returns the height at (x, z)
	float getHeight(int x, int z) {
		return hs[z][x];
	}

	//Computes the normals, if they haven't been computed yet
	void computeNormals() {
		if (computedNormals) {
			return;
		}

		//Compute the rough version of the normals
		Vec3f** normals2 = new Vec3f*[l];
		for (int i = 0; i < l; i++) {
			normals2[i] = new Vec3f[w];
		}

		for (int z = 0; z < l; z++) {
			for (int x = 0; x < w; x++) {
				Vec3f sum(0.0f, 0.0f, 0.0f);

				Vec3f out;
				if (z > 0) {
					out = Vec3f(0.0f, hs[z - 1][x] - hs[z][x], -1.0f);
				}
				Vec3f in;
				if (z < l - 1) {
					in = Vec3f(0.0f, hs[z + 1][x] - hs[z][x], 1.0f);
				}
				Vec3f left;
				if (x > 0) {
					left = Vec3f(-1.0f, hs[z][x - 1] - hs[z][x], 0.0f);
				}
				Vec3f right;
				if (x < w - 1) {
					right = Vec3f(1.0f, hs[z][x + 1] - hs[z][x], 0.0f);
				}

				if (x > 0 && z > 0) {
					sum += out.cross(left).normalize();
				}
				if (x > 0 && z < l - 1) {
					sum += left.cross(in).normalize();
				}
				if (x < w - 1 && z < l - 1) {
					sum += in.cross(right).normalize();
				}
				if (x < w - 1 && z > 0) {
					sum += right.cross(out).normalize();
				}

				normals2[z][x] = sum;
			}
		}

		//Smooth out the normals
		const float FALLOUT_RATIO = 0.5f;//memperhalus
		for (int z = 0; z < l; z++) {
			for (int x = 0; x < w; x++) {
				Vec3f sum = normals2[z][x];

				if (x > 0) {
					sum += normals2[z][x - 1] * FALLOUT_RATIO;
				}
				if (x < w - 1) {
					sum += normals2[z][x + 1] * FALLOUT_RATIO;
				}
				if (z > 0) {
					sum += normals2[z - 1][x] * FALLOUT_RATIO;
				}
				if (z < l - 1) {
					sum += normals2[z + 1][x] * FALLOUT_RATIO;
				}

				if (sum.magnitude() == 0) {
					sum = Vec3f(0.0f, 1.0f, 0.0f);
				}
				normals[z][x] = sum;
			}
		}

		for (int i = 0; i < l; i++) {
			delete[] normals2[i];
		}
		delete[] normals2;

		computedNormals = true;
	}

	//Returns the normal at (x, z)
	Vec3f getNormal(int x, int z) {
		if (!computedNormals) {
			computeNormals();
		}
		return normals[z][x];
	}
};
//end class
//Loads a terrain from a heightmap.  The heights of the terrain range from
//-height / 2 to height / 2.
//load terain di procedure inisialisasi
Terrain* loadTerrain(const char* filename, float height) {//ngambil file
	Image* image = loadBMP(filename);
	Terrain* t = new Terrain(image->width, image->height);
	for (int y = 0; y < image->height; y++) {
		for (int x = 0; x < image->width; x++) {
			unsigned char color = (unsigned char) image->pixels[3 * (y
					* image->width + x)];
			float h = height * ((color / 255.0f) - 0.5f);
			t->setHeight(x, y, h);
		}
	}

	delete image;
	t->computeNormals();
	return t;
}

float _angle = 60.0f;
//buat tipe data terain
Terrain* _terrainAir;
Terrain* _terrain;//pariable terain
Terrain* _terrainTanah;
Terrain* _terrainJalan;


void cleanup() {//untuk mehilangin file image
    delete _terrainJalan;
    delete _terrainAir;
	delete _terrain;
	delete _terrainTanah;
}
//mengambil gambar BMP
int ImageLoad(char *filename, Images *image) {
	FILE *file;
	unsigned long size; // ukuran image dalam bytes
	unsigned long i; // standard counter.
	unsigned short int plane; // number of planes in image

	unsigned short int bpp; // jumlah bits per pixel
	char temp; // temporary color storage for var warna sementara untuk memastikan filenya ada


	if ((file = fopen(filename, "rb")) == NULL) {
		printf("File Not Found : %s\n", filename);
		return 0;
	}
	// mencari file header bmp
	fseek(file, 18, SEEK_CUR);
	// read the width
	if ((i = fread(&image->sizeX, 4, 1, file)) != 1) {//lebar beda
		printf("Error reading width from %s.\n", filename);
		return 0;
	}
	//printf("Width of %s: %lu\n", filename, image->sizeX);
	// membaca nilai height
	if ((i = fread(&image->sizeY, 4, 1, file)) != 1) {//tingginya beda
		printf("Error reading height from %s.\n", filename);
		return 0;
	}
	//printf("Height of %s: %lu\n", filename, image->sizeY);
	//menghitung ukuran image(asumsi 24 bits or 3 bytes per pixel).

	size = image->sizeX * image->sizeY * 3;
	// read the planes
	if ((fread(&plane, 2, 1, file)) != 1) {
		printf("Error reading planes from %s.\n", filename);//bukan file bmp
		return 0;
	}
	if (plane != 1) {
		printf("Planes from %s is not 1: %u\n", filename, plane);//
		return 0;
	}
	// read the bitsperpixel
	if ((i = fread(&bpp, 2, 1, file)) != 1) {
		printf("Error reading bpp from %s.\n", filename);

		return 0;
	}
	if (bpp != 24) {
		printf("Bpp from %s is not 24: %u\n", filename, bpp);//bukan 24 pixel
		return 0;
	}
	// seek past the rest of the bitmap header.
	fseek(file, 24, SEEK_CUR);
	// read the data.
	image->data = (char *) malloc(size);
	if (image->data == NULL) {
		printf("Error allocating memory for color-corrected image data");//gagal ambil memory
		return 0;
	}
	if ((i = fread(image->data, size, 1, file)) != 1) {
		printf("Error reading image data from %s.\n", filename);
		return 0;
	}
	for (i = 0; i < size; i += 3) { // membalikan semuan nilai warna (gbr - > rgb)
		temp = image->data[i];
		image->data[i] = image->data[i + 2];
		image->data[i + 2] = temp;
	}
	// we're done.
	return 1;
}




//mengambil tekstur
Images * loadTexture() {
	Images *image1;
	// alokasi memmory untuk tekstur
	image1 = (Images *) malloc(sizeof(Images));
	if (image1 == NULL) {
		printf("Error allocating space for image");//memory tidak cukup
		exit(0);
	}
	//pic.bmp is a 64x64 picture
	if (!ImageLoad("Wood-10777.bmp", image1)) {
		exit(1);
	}
	return image1;
}



void initRendering() {//inisialisasi
	glEnable(GL_DEPTH_TEST);//kedalaman
	glEnable(GL_COLOR_MATERIAL);//warna
	glEnable(GL_LIGHTING);//cahaya
	glEnable(GL_LIGHT0);//lampu
	glEnable(GL_NORMALIZE);
	glShadeModel(GL_SMOOTH);
}

void drawScene() {//buat terain
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	float scale = 500.0f / max(_terrain->width() - 1, _terrain->length() - 1);
	glScalef(scale, scale, scale);
	glTranslatef(-(float) (_terrain->width() - 1) / 2, 0.0f,
			-(float) (_terrain->length() - 1) / 2);

	glColor3f(0.3f, 0.9f, 0.0f);
	for (int z = 0; z < _terrain->length() - 1; z++) {
		//Makes OpenGL draw a triangle at every three consecutive vertices
		glBegin(GL_TRIANGLE_STRIP);
		for (int x = 0; x < _terrain->width(); x++) {
			Vec3f normal = _terrain->getNormal(x, z);
			glNormal3f(normal[0], normal[1], normal[2]);
			glVertex3f(x, _terrain->getHeight(x, z), z);
			normal = _terrain->getNormal(x, z + 1);
			glNormal3f(normal[0], normal[1], normal[2]);
			glVertex3f(x, _terrain->getHeight(x, z + 1), z + 1);
		}
		glEnd();
	}

}


//untuk di display
void drawSceneTanah(Terrain *terrain, GLfloat r, GLfloat g, GLfloat b) {

	float scale = 500.0f / max(terrain->width() - 1, terrain->length() - 1);
	glScalef(scale, scale, scale);
	glTranslatef(-(float) (terrain->width() - 1) / 2, 0.0f,
			-(float) (terrain->length() - 1) / 2);

	glColor3f(r, g, b);
	for (int z = 0; z < terrain->length() - 1; z++) {
		//Makes OpenGL draw a triangle at every three consecutive vertices
		glBegin(GL_TRIANGLE_STRIP);
		for (int x = 0; x < terrain->width(); x++) {
			Vec3f normal = terrain->getNormal(x, z);
			glNormal3f(normal[0], normal[1], normal[2]);
			glVertex3f(x, terrain->getHeight(x, z), z);
			normal = terrain->getNormal(x, z + 1);
			glNormal3f(normal[0], normal[1], normal[2]);
			glVertex3f(x, terrain->getHeight(x, z + 1), z + 1);
		}
		glEnd();
	}

}

GLuint loadtextures(const char *filename, int width, int height) {//buat ngambil dari file image untuk jadi texture
	GLuint texture;

	unsigned char *data;
	FILE *file;


	file = fopen(filename, "rb");
	if (file == NULL)
		return 0;

	data = (unsigned char *) malloc(width * height * 3);  //file pendukung texture
	fread(data, width * height * 3, 1, file);

	fclose(file);

	glGenTextures(1, &texture);//generet (dibentuk)
	glBindTexture(GL_TEXTURE_2D, texture);//binding (gabung)
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
			GL_LINEAR_MIPMAP_NEAREST);//untuk membaca gambar jadi text dan dapat dibaca dengan pixel
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	//glTexParameterf(  GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	//glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	gluBuild2DMipmaps(GL_TEXTURE_2D, 3, width, height, GL_RGB,
			GL_UNSIGNED_BYTE, data);

	data = NULL;

	return texture;
}

const GLfloat light_ambient[] = { 0.3f, 0.3f, 0.3f, 1.0f };
const GLfloat light_diffuse[] = { 0.7f, 0.7f, 0.7f, 1.0f };
const GLfloat light_specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
const GLfloat light_position[] = { 1.0f, 1.0f, 1.0f, 1.0f };

const GLfloat light_ambient2[] = { 0.3f, 0.3f, 0.3f, 0.0f };
const GLfloat light_diffuse2[] = { 0.3f, 0.3f, 0.3f, 0.0f };

const GLfloat mat_ambient[] = { 0.8f, 0.8f, 0.8f, 1.0f };
const GLfloat mat_diffuse[] = { 0.8f, 0.8f, 0.8f, 1.0f };
const GLfloat mat_specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
const GLfloat high_shininess[] = { 100.0f };

void rumah(void) {
 
   //atap
    glPushMatrix();
    glScaled(0.8, 1.0, 0.8);
    glTranslatef(0.0, 4.85, -1.9);
    glRotated(45, 0, 1, 0);
    glRotated(-90, 1, 0, 0);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3d(0.803921568627451, 0.5215686274509804, 0.2470588235294118);
    glutSolidCone(4.2, 1.5, 4, 1);
    glPopMatrix();
  
   //atap
    glPushMatrix();
    glScaled(0.8, 1.0, 0.8);
    glTranslatef(0.0, 4.85, 2.1);
    glRotated(45, 0, 1, 0);
    glRotated(-90, 1, 0, 0);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3d(0.803921568627451, 0.5215686274509804, 0.2470588235294118);
    glutSolidCone(4.2, 1.5, 4, 1);
    glPopMatrix();
  

   //lantai 1  
    glPushMatrix();
    glScaled(1.115, 0.03, 2.2);
    glTranslatef(0.0, 0, 0.0);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.3402, 0.3412, 0.3117);
    glutSolidCube(5.0);
    glPopMatrix(); 
    
    //lantai 2 depan
    glPushMatrix();
    glScaled(1.015, 0.03, 1.2);
    glTranslatef(0.0,80, 1.7);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.4613, 0.4627, 0.4174);
    glutSolidCube(5.0);
    glPopMatrix(); 
    
    //lantai 2 belakang
    glPushMatrix();
    glScaled(0.5, 0.03, 0.8);
    glTranslatef(2.5,80, -2.8);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.4613, 0.4627, 0.4174);
    glutSolidCube(5.0);
    glPopMatrix(); 
    
    //lantai 3 
    glPushMatrix();
    glScaled(1.015, 0.03, 1.8);
    glTranslatef(0.0,160, 0.3);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
  glColor3f(0.3402, 0.3412, 0.3117);
    glutSolidCube(5.0);
    glPopMatrix(); 
    
    
  //Dinding Kiri Bawah
    glPushMatrix();
    glScaled(0.035, 0.5, 1.6);
    glTranslatef(-70.0, 2.45, 0.0);   
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.4613, 0.4627, 0.4174);
    glutSolidCube(5.0);
    glPopMatrix();
  
  //Dinding Kanan Bawah
    glPushMatrix();
    glScaled(0.035, 0.5, 1.6);
    glTranslatef(70.0, 2.45, 0.0);  
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.4613, 0.4627, 0.4174);  
    glutSolidCube(5.0);
    glPopMatrix();
  
//Dinding Kiri Atas
    glPushMatrix();
    glScaled(0.035, 0.5, 1.8);
    glTranslatef(-70.0, 7.45, 0.3);   
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.4613, 0.4627, 0.4174); 
    glutSolidCube(5.0);
    glPopMatrix();
  
//Dinding Kanan Atas
    glPushMatrix();
    glScaled(0.035, 0.5, 1.8);
    glTranslatef(70.0, 7.45, 0.3); 
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.4613, 0.4627, 0.4174);  
    glutSolidCube(5.0);
    glPopMatrix();  
   
  //Dinding Belakang bawah
    glPushMatrix();
    //glScaled(0.035, 0.5, 0.8);
    glScaled(1.015, 0.5, 0.07);
    glTranslatef(0, 2.45,-58);  
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.4613, 0.4627, 0.4174);  
    glutSolidCube(5.0);
    glPopMatrix();
    
  //Dinding Belakang atas
    glPushMatrix();
    //glScaled(0.035, 0.5, 0.8);
    glScaled(1.015, 0.5, 0.07);
    glTranslatef(0, 7.45,-58);  
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.4613, 0.4627, 0.4174);  
    glutSolidCube(5.0);
    glPopMatrix();
    
  //Dinding Depan bawah
    glPushMatrix();
    glScaled(1.015, 0.5, 0.035);
    glTranslatef(0, 2.45,116); 
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.4613, 0.4627, 0.4174);   
    glutSolidCube(5.0);
    glPopMatrix();

  //Dinding Depan atas
    glPushMatrix();
    glScaled(1.015, 0.5, 0.035);
    glTranslatef(0, 7.45,116);  
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.4613, 0.4627, 0.4174);  
    glutSolidCube(5.0);
    glPopMatrix();
     

  //list hitam atas
    glPushMatrix();
    glScaled(0.35, 0.5, 0.035);
   glTranslatef(1, 7.2,124); 
   glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.1412, 0.1389, 0.1356);
    glutSolidCube(5.0);
    glPopMatrix();

  //list hitam atas
    glPushMatrix();
    glScaled(0.35, 0.43, 0.035);
   glTranslatef(1, 3.5,124); 
   glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.1412, 0.1389, 0.1356);
    glutSolidCube(5.0);
    glPopMatrix();

    //pintu atas
    glPushMatrix();
    glScaled(0.18, 0.35, 0.035);
   glTranslatef(-8, 9.5,118); 
   glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.0980, 0.0608, 0.0077);
    glutSolidCube(5.0);
    glPopMatrix();   
    
  //pintu bawah
    glPushMatrix();
    glScaled(0.18, 0.35, 0.035);
   glTranslatef(-8, 2.5,118); 
   glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.0980, 0.0608, 0.0077);
    glutSolidCube(5.0);
    glPopMatrix();
    
   //alis
    glPushMatrix();
    glScaled(0.18, 0.017, 0.035);
   glTranslatef(-8, 110.5,118); 
   glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
     glColor3f(0, 0, 0);
    glutSolidCube(5.0);
    glPopMatrix();
    
   //alis atas kiri
    glPushMatrix();
    glScaled(0.18, 0.017, 0.035);
   glTranslatef(-8, 254,118); 
   glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
     //glColor3f(0.3402, 0.3412, 0.3117);
     glColor3f(0, 0, 0);
    glutSolidCube(5.0);
    glPopMatrix();
    
   //alis atas kanan
    glPushMatrix();
    glScaled(0.10, 0.017, 0.035);
   glTranslatef(18, 254,118); 
   glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
       glColor3f(0, 0, 0);
    glutSolidCube(5.0);
    glPopMatrix();    
 
   //alis jedela atas
    glPushMatrix();
    glScaled(0.08, 0.017, 0.035);
   glTranslatef(22.5, 245,118); 
   glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
     glColor3f(0.3402, 0.3412, 0.3117);
    glutSolidCube(5.0);
    glPopMatrix();      
  
    //alis jedela bawah
    glPushMatrix();
    glScaled(0.08, 0.017, 0.035);
   glTranslatef(22.5, 165,118); 
   glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
     glColor3f(0.3402, 0.3412, 0.3117);
    glutSolidCube(5.0);
    glPopMatrix();     
  
    //alis jedela kiri
    glPushMatrix();
    glScaled(0.017,0.28, 0.035);
   glTranslatef(96.6, 12.5,118); 
   glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
     glColor3f(0.3402, 0.3412, 0.3117);
    glutSolidCube(5.0);
    glPopMatrix();   
   
    //alis jedela kanan
    glPushMatrix();
    glScaled(0.017,0.28, 0.035);
   glTranslatef(115.1, 12.5,118); 
   glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
     glColor3f(0.3402, 0.3412, 0.3117);
    glutSolidCube(5.0);
    glPopMatrix();
    
//jendela bawah (kanan Bawah)
   //alis atas kanan (kanan Bawah)
    glPushMatrix();
    glScaled(0.10, 0.017, 0.035);
   glTranslatef(18, 110.5,118); 
   glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
       glColor3f(0, 0, 0);
    glutSolidCube(5.0);
    glPopMatrix();  
    
//alis jedela atas (kanan Bawah)
    glPushMatrix();
    glScaled(0.08, 0.017, 0.035);
   glTranslatef(22.5, 101.5,118); 
   glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
     glColor3f(0.3402, 0.3412, 0.3117);
    glutSolidCube(5.0);
    glPopMatrix();      
  
    //alis jedela bawah (kanan Bawah)
    glPushMatrix();
    glScaled(0.08, 0.017, 0.035);
   glTranslatef(22.5, 22.0,118); 
   glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
     glColor3f(0.3402, 0.3412, 0.3117);
    glutSolidCube(5.0);
    glPopMatrix();     
  
    //alis jedela kiri (kanan Bawah)
    glPushMatrix();
    glScaled(0.017,0.28, 0.035);
   glTranslatef(96.6, 3.8,118); 
   glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
     glColor3f(0.3402, 0.3412, 0.3117);
    glutSolidCube(5.0);
    glPopMatrix();   
   
    //alis jedela kanan (kanan Bawah)
    glPushMatrix();
    glScaled(0.017,0.28, 0.035);
   glTranslatef(115.1, 3.8,118); 
   glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
     glColor3f(0.3402, 0.3412, 0.3117);
    glutSolidCube(5.0);
    glPopMatrix();        
     
 
    
//alis jedela atas (tengah1)
    glPushMatrix();
    glScaled(0.08, 0.017, 0.035);
   glTranslatef(0, 119.5,128); 
   glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
     glColor3f(1.0000, 0.5252, 0.0157);
    glutSolidCube(5.0);
    glPopMatrix();  
    
    //alis jedela bawah (tengah1)
    glPushMatrix();
    glScaled(0.08, 0.017, 0.035);
   glTranslatef(0, 40.0,128); 
   glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
     glColor3f(1.0000, 0.5252, 0.0157);
    glutSolidCube(5.0);
    glPopMatrix();     
  
    //alis jedela kiri (tengah1)
    glPushMatrix();
    glScaled(0.017,0.28, 0.035);
   glTranslatef(-9.6, 4.8,128); 
   glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
     glColor3f(1.0000, 0.5252, 0.0157);
    glutSolidCube(5.0);
    glPopMatrix();   
   
    //alis jedela kanan (tengah1)
    glPushMatrix();
    glScaled(0.017,0.28, 0.035);
   glTranslatef(9.5, 4.8,128); 
   glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
     glColor3f(1.0000, 0.5252, 0.0157);
    glutSolidCube(5.0);
    glPopMatrix(); 

//alis jedela atas (tengah2)
    glPushMatrix();
    glScaled(0.08, 0.017, 0.035);
   glTranslatef(9, 119.5,128); 
   glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
     glColor3f(1.0000, 0.5252, 0.0157);
    glutSolidCube(5.0);
    glPopMatrix();  
    
    //alis jedela bawah (tengah2)
    glPushMatrix();
    glScaled(0.08, 0.017, 0.035);
   glTranslatef(9, 40.0,128); 
   glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
     glColor3f(1.0000, 0.5252, 0.0157);
    glutSolidCube(5.0);
    glPopMatrix();     
  
    //alis jedela kiri (tengah2)
    glPushMatrix();
    glScaled(0.017,0.28, 0.035);
   glTranslatef(33, 4.8,128); 
   glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
     glColor3f(1.0000, 0.5252, 0.0157);
    glutSolidCube(5.0);
    glPopMatrix();   
   
    //alis jedela kanan (tengah2)
    glPushMatrix();
    glScaled(0.017,0.28, 0.035);
   glTranslatef(51.7, 4.8,128); 
   glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
     glColor3f(1.0000, 0.5252, 0.0157);
    glutSolidCube(5.0);
    glPopMatrix();     
    
    
    
   //alis tiang kiri atas orange
    glPushMatrix();
    glScaled(0.06, 0.037, 0.095);
   glTranslatef(-41, 115,51.5); 
   glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
     glColor3f(1.0000, 0.5252, 0.0157);
    glutSolidCube(5.0);
    glPopMatrix();
        
   //alis tiang kiri bawah orange
    glPushMatrix();
    glScaled(0.06, 0.037, 0.095);
   glTranslatef(-41, 80,51.5); 
   glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
     glColor3f(1.0000, 0.5252, 0.0157);
    glutSolidCube(5.0);
    glPopMatrix();     

    //alis tiang kanan atas orange
    glPushMatrix();
    glScaled(0.06, 0.037, 0.095);
   glTranslatef(41, 115,51.5); 
   glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
     glColor3f(1.0000, 0.5252, 0.0157);
    glutSolidCube(5.0);
    glPopMatrix();
    
   //alis tiang kanan bawah orange
    glPushMatrix();
    glScaled(0.06, 0.037, 0.095);
   glTranslatef(41, 80,51.5); 
   glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
     glColor3f(1.0000, 0.5252, 0.0157);
    glutSolidCube(5.0);
    glPopMatrix();      

    //orange 3 di tengah
    glPushMatrix();
    glScaled(0.017,0.33, 0.035);
   glTranslatef(-16.6, 12,125); 
   glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
     glColor3f(1.0000, 0.5252, 0.0157);
    glutSolidCube(5.0);
    glPopMatrix();     

    //orange 3 di tengah
    glPushMatrix();
    glScaled(0.017,0.33, 0.035);
   glTranslatef(-6.6, 12,125); 
   glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
     glColor3f(1.0000, 0.5252, 0.0157);
    glutSolidCube(5.0);
    glPopMatrix();  
    
//orange 3 di tengah
    glPushMatrix();
    glScaled(0.017,0.33, 0.035);
   glTranslatef(3.6, 12,125); 
   glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
     glColor3f(1.0000, 0.5252, 0.0157);
    glutSolidCube(5.0);
    glPopMatrix();  

    //pagar atas 1
    glPushMatrix();
    glScaled(.88, 0.017, 0.017);
   glTranslatef(-.01, 149,290); 
   glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
     glColor3f(1,1,1);
    glutSolidCube(5.0);
    glPopMatrix(); 


    //pagar atas 1
    glPushMatrix();
    glScaled(.88, 0.017, 0.017);
   glTranslatef(-.01, 159,290); 
   glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
     glColor3f(1.0000, 0.5252, 0.0157);
    glutSolidCube(5.0);
    glPopMatrix();   
    
    //pagar atas 1
    glPushMatrix();
    glScaled(.88, 0.017, 0.017);
   glTranslatef(-.01, 169,290); 
   glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
     glColor3f(1,1,1);
    glutSolidCube(5.0);
    glPopMatrix(); 
       
    //pagar atas 1
    glPushMatrix();
    glScaled(.88, 0.017, 0.017);
   glTranslatef(-.01, 179,290); 
   glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
     glColor3f(0.3402, 0.3412, 0.3117);
    glutSolidCube(5.0);
    glPopMatrix(); 
       
  
    //lampu kanan atas
    glPushMatrix();
    glScaled(0.05, 0.05, 0.05);
    glTranslatef(34.5, 95.4, 96);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE); //untuk memunculkan warna
    glColor3ub(252, 243, 169);
    glutSolidSphere(2.0,20,50);
    glPopMatrix();       
      
    //lampu kiri atas
    glPushMatrix();
    glScaled(0.05, 0.05, 0.05);
    glTranslatef(-32.5, 95.4, 96);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3ub(252, 243, 169);
    glutSolidSphere(2.0,20,50);
    glPopMatrix();        
  


    //lampu kanan atas
    glPushMatrix();
    glScaled(0.05, 0.05, 0.05);
    glTranslatef(34.5, 47, 96);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE); //untuk memunculkan warna
    glColor3ub(252, 243, 169);
    glutSolidSphere(2.0,20,50);
    glPopMatrix();       
      
    //lampu kiri atas
    glPushMatrix();
    glScaled(0.05, 0.05, 0.05);
    glTranslatef(-32.5, 47, 96);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3ub(252, 243, 169);
    glutSolidSphere(2.0,20,50);
    glPopMatrix(); 
    
    //pagar bawah
    glPushMatrix();
    glScaled(.7, 0.017, 0.017);
   glTranslatef(1, 50,400); 
   glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
     glColor3f(1,1,1);
    glutSolidCube(5.0);
    glPopMatrix(); 
    
    //pagar bawah
    glPushMatrix();
    glScaled(.7, 0.017, 0.017);
   glTranslatef(1, 40,400); 
   glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
     glColor3f(1,1,1);
    glutSolidCube(5.0);
    glPopMatrix(); 
    
    //pagar bawah
    glPushMatrix();
    glScaled(.7, 0.017, 0.017);
   glTranslatef(1, 30,400); 
   glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
     glColor3f(1,1,1);
    glutSolidCube(5.0);
    glPopMatrix(); 
    
    //pagar bawah
    glPushMatrix();
    glScaled(.7, 0.017, 0.017);
   glTranslatef(1, 20,400); 
   glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
     glColor3f(1,1,1);
    glutSolidCube(5.0);
    glPopMatrix(); 
        
   //pagar bawah
    glPushMatrix();
    glScaled(.7, 0.017, 0.017);
   glTranslatef(1, 10,400); 
   glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
     glColor3f(1,1,1);
    glutSolidCube(5.0);
    glPopMatrix(); 
            
    // Batang Tiang Kanan
    glPushMatrix();
    glScaled(0.06, 0.2,0.06);
   glTranslatef(43, 3,115.5); 
   glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
     glColor3f(1.0000, 0.5252, 0.0157);
    glutSolidCube(5.0);
    glPopMatrix();    
    
    // Batang Tiang Kiri 1
    glPushMatrix();
    glScaled(0.06, 0.2,0.06);
   glTranslatef(-42, 3,115.5); 
   glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
     glColor3f(1.0000, 0.5252, 0.0157);
    glutSolidCube(5.0);
    glPopMatrix();    
    
    // Batang Tiang Kiri 2
    glPushMatrix();
    glScaled(0.06, 0.2,0.06);
   glTranslatef(-20, 3,115.5); 
   glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
     glColor3f(1.0000, 0.5252, 0.0157);
    glutSolidCube(5.0);
    glPopMatrix();      
        
     
}

void pohon(void){
//batang
GLUquadricObj *pObj;
pObj =gluNewQuadric();
gluQuadricNormals(pObj, GLU_SMOOTH);    

glPushMatrix();
glColor3ub(104,70,14);
glRotatef(270,1,0,0);
gluCylinder(pObj, 4, 0.7, 30, 25, 25);
glPopMatrix();
}

//ranting  
void ranting(void){
GLUquadricObj *pObj;
pObj =gluNewQuadric();
gluQuadricNormals(pObj, GLU_SMOOTH); 
glPushMatrix();
glColor3ub(104,70,14);
glTranslatef(0,27,0);
glRotatef(330,1,0,0);
gluCylinder(pObj, 0.6, 0.1, 15, 25, 25);
glPopMatrix();

//daun
glPushMatrix();
glColor3ub(18,118,13);
glScaled(5, 5, 5);
glTranslatef(0,7,3);
glutSolidDodecahedron();
glPopMatrix();
}


void display(void){
//    glutSwapBuffers();
	glClearStencil(0); //clear the stencil buffer
	glClearDepth(1.0f);

	glClearColor(0.0, 0.6, 0.8, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glLoadIdentity();//reset posisi
	gluLookAt(viewx, viewy, viewz, 0.0, 0.0, -100.0, 0.0, 1.0, 0.0);


    glPushMatrix();
    drawScene();
    glPopMatrix();
    
    glPushMatrix();

	glBindTexture(GL_TEXTURE_2D, texture[1]); //untuk mmanggil texture
	//drawSceneTanah(_terrain, 0.804f, 0.53999999f, 0.296f);
	drawSceneTanah(_terrain, 0.3f, 0.53999999f, 0.0654f);
	glPopMatrix();

	glPushMatrix();
        
        drawSceneTanah(_terrainJalan, 0.4902f, 0.4683f,0.4594f);
        glPopMatrix();
	
	glPushMatrix();
	drawSceneTanah(_terrainAir, 0.0f, 0.2f, 0.5f);
	glPopMatrix();
        

	glPushMatrix();

//	glBindTexture(GL_TEXTURE_2D, texture[0]);
	drawSceneTanah(_terrainTanah, 0.7f, 0.2f, 0.1f);
	glPopMatrix();



//rumah 1
glPushMatrix();
glTranslatef(0,5,-10); 
glScalef(5, 5, 5);
//glBindTexture(GL_TEXTURE_2D, texture[0]);
rumah();
glPopMatrix();



//pohon2
glPushMatrix();
glTranslatef(35,0.5,-10);    
glScalef(0.5, 0.5, 0.5);
glRotatef(90,0,1,0);
pohon();

//ranting1
ranting();

//ranting2
glPushMatrix();
glScalef(1.5, 1.5, 1.5);
glTranslatef(0,25,25);   
glRotatef(250,1,0,0);
ranting();
glPopMatrix();

//ranting3
glPushMatrix();
glScalef(1.8, 1.8, 1.8);
glTranslatef(0,-6,21.5);   
glRotatef(-55,1,0,0);
ranting();
glPopMatrix();

glPopMatrix();

//pohon3
glPushMatrix();
glTranslatef(55,0.2,-40);    
glScalef(0.7, 0.8, 0.8);
glRotatef(90,0,1,0);
pohon();

//ranting1
ranting();

//ranting2
glPushMatrix();
glScalef(1.5, 1.5, 1.5);
glTranslatef(0,25,25);   
glRotatef(250,1,0,0);
ranting();
glPopMatrix();

//ranting3
glPushMatrix();
glScalef(1.8, 1.8, 1.8);
glTranslatef(0,-6,21.5);   
glRotatef(-55,1,0,0);
ranting();
glPopMatrix();

glPopMatrix();

//pohon4
glPushMatrix();
glTranslatef(145,0.2,-17);    
glScalef(0.7, 0.8, 0.8);
glRotatef(90,0,1,0);
pohon();

//ranting1 4
ranting();

//ranting2 4
glPushMatrix();
glScalef(1.5, 1.5, 1.5);
glTranslatef(0,25,25);   
glRotatef(250,1,0,0);
ranting();
glPopMatrix();

//ranting3 4
glPushMatrix();
glScalef(1.8, 1.8, 1.8);
glTranslatef(0,-6,21.5);   
glRotatef(-55,1,0,0);
ranting();
glPopMatrix();

glPopMatrix();

//pohon5
glPushMatrix();
glTranslatef(115,0.5,-60);    
glScalef(0.5, 0.5, 0.5);
glRotatef(90,0,1,0);
pohon();

//ranting1 5
ranting();

//ranting2 5
glPushMatrix();
glScalef(1.5, 1.5, 1.5);
glTranslatef(0,25,25);   
glRotatef(250,1,0,0);
ranting();
glPopMatrix();

//ranting3 5
glPushMatrix();
glScalef(1.8, 1.8, 1.8);
glTranslatef(0,-6,21.5);   
glRotatef(-55,1,0,0);
ranting();
glPopMatrix();

glPopMatrix();

//pohon6
glPushMatrix();
glTranslatef(35,0.5,100);    
glScalef(0.5, 0.5, 0.5);
glRotatef(90,0,1,0);
pohon();

//ranting1 6
ranting();

//ranting2 6
glPushMatrix();
glScalef(1.5, 1.5, 1.5);
glTranslatef(0,25,25);   
glRotatef(250,1,0,0);
ranting();
glPopMatrix();

//ranting3 6
glPushMatrix();
glScalef(1.8, 1.8, 1.8);
glTranslatef(0,-6,21.5);   
glRotatef(-55,1,0,0);
ranting();
glPopMatrix();

glPopMatrix();

//pohon7
glPushMatrix();
glTranslatef(-15,0.5,100);    
glScalef(0.5, 0.5, 0.5);
glRotatef(90,0,1,0);
pohon();

//ranting1 7
ranting();

//ranting2 7
glPushMatrix();
glScalef(1.5, 1.5, 1.5);
glTranslatef(0,25,25);   
glRotatef(250,1,0,0);
ranting();
glPopMatrix();

//ranting3 7
glPushMatrix();
glScalef(1.8, 1.8, 1.8);
glTranslatef(0,-6,21.5);   
glRotatef(-55,1,0,0);
ranting();
glPopMatrix();

glPopMatrix();

//pohon8
glPushMatrix();
glTranslatef(-65,0.5,100);    
glScalef(0.5, 0.5, 0.5);
glRotatef(90,0,1,0);
pohon();

//ranting1 8
ranting();

//ranting2 8
glPushMatrix();
glScalef(1.5, 1.5, 1.5);
glTranslatef(0,25,25);   
glRotatef(250,1,0,0);
ranting();
glPopMatrix();

//ranting3 8
glPushMatrix();
glScalef(1.8, 1.8, 1.8);
glTranslatef(0,-6,21.5);   
glRotatef(-55,1,0,0);
ranting();
glPopMatrix();

glPopMatrix();









    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE); //disable the color mask
	glDepthMask(GL_FALSE); //disable the depth mask

	glEnable(GL_STENCIL_TEST); //enable the stencil testing

	glStencilFunc(GL_ALWAYS, 1, 0xFFFFFFFF);
	glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE); //set the stencil buffer to replace our next lot of data

	//ground
	//tanah(); //set the data plane to be replaced
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE); //enable the color mask
	glDepthMask(GL_TRUE); //enable the depth mask

	glStencilFunc(GL_EQUAL, 1, 0xFFFFFFFF);
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP); //set the stencil buffer to keep our next lot of data

	glDisable(GL_DEPTH_TEST); //disable depth testing of the reflection

	// glPopMatrix();  
	glEnable(GL_DEPTH_TEST); //enable the depth testing
	glDisable(GL_STENCIL_TEST); //disable the stencil testing
	//end of ground
	glEnable(GL_BLEND); //enable alpha blending
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); //set the blending function
	glRotated(1, 0, 0, 0);
	
	glDisable(GL_BLEND);



    glutSwapBuffers();//buffeer ke memory
	glFlush();//memaksa untuk menampilkan
	rot++;
	angle++;




//glDisable(GL_COLOR_MATERIAL);
}

void init(void){
/*GLfloat mat_specular[] = {1.0, 1.0, 1.0, 1.0};
GLfloat mat_shininess[] = {50.0};
GLfloat light_position[] = {1.0, 1.0, 1.0, 1.0};
     
glClearColor(0.0,0.0,0.0,0.0);

//glShadeModel(GL_FLAT);

glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
glMaterialfv(GL_FRONT, GL_SHININESS, mat_shininess);
glLightfv(GL_LIGHT0, GL_POSITION, light_position);
*/

glEnable(GL_DEPTH_TEST);
glEnable(GL_LIGHTING);
glEnable(GL_LIGHT0);
glDepthFunc(GL_LESS);
glEnable(GL_NORMALIZE);
glEnable(GL_COLOR_MATERIAL);
glDepthFunc(GL_LEQUAL);
glShadeModel(GL_SMOOTH);
glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
glEnable(GL_CULL_FACE);
glEnable(GL_TEXTURE_2D);
glEnable(GL_TEXTURE_GEN_S);
glEnable(GL_TEXTURE_GEN_T);


    initRendering();
        _terrainJalan = loadTerrain("heightmapJalan.bmp", 20);
        _terrainAir = loadTerrain("heightmapAir.bmp", 20);
	    _terrain = loadTerrain("heightmap02.bmp", 20);
	    _terrainTanah = loadTerrain("heightmapTanah.bmp", 20);
        
	//binding texture

Images *image1 = loadTexture();

if (image1 == NULL) {
		printf("Image was not returned from loadTexture\n");
		exit(0);
	}
	
glPixelStorei(GL_UNPACK_ALIGNMENT, 1);


	// Generate texture/ membuat texture
	glGenTextures(1, texture);


	
//------------tekstur rumah---------------

	//binding texture untuk membuat texture 2D
	glBindTexture(GL_TEXTURE_2D, texture[0]);


    
	//menyesuaikan ukuran textur ketika image lebih besar dari texture
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); //
	//menyesuaikan ukuran textur ketika image lebih kecil dari texture
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); //

	glTexImage2D(GL_TEXTURE_2D, 0, 3, image1->sizeX, image1->sizeY, 0, GL_RGB,
			GL_UNSIGNED_BYTE, image1->data);
 // texture
}

void reshape(int w, int h){
glViewport(0, 0 , (GLsizei) w,(GLsizei)h);
glMatrixMode(GL_PROJECTION);
glLoadIdentity();

//glFrustum(-1.0,1.0,-1.0,1.0,1.5,20.0);
gluPerspective(60, (GLfloat) w / (GLfloat) h, 0.1, 1000.0);
glMatrixMode(GL_MODELVIEW);
//glLoadIdentity();
//gluLookAt(100.0,0.0,5.0,0.0,0.0,0.0,0.0,1.0,0.0);
}

static void kibor(int key, int x, int y) {
	switch (key) {
	case GLUT_KEY_HOME:
		viewy++;
		break;
	case GLUT_KEY_END:
		viewy--;
		break;
	case GLUT_KEY_UP:
		viewz--;
		break;
	case GLUT_KEY_DOWN:
		viewz++;
		break;

	case GLUT_KEY_RIGHT:
		viewx++;
		break;
	case GLUT_KEY_LEFT:
		viewx--;
		break;

	case GLUT_KEY_F1: {
		glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
		glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
		glMaterialfv(GL_FRONT, GL_AMBIENT, mat_ambient);
		glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
	}
		;
		break;
	case GLUT_KEY_F2: {
		glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient2); //untuk lighting
		glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse2);
		glMaterialfv(GL_FRONT, GL_AMBIENT, mat_ambient);
		glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
	}
		;
		break;
	default:
		break;
	}
}

void keyboard(unsigned char key, int x, int y) {
	if (key == 'q') {
		viewz++;
	}
	if (key == 'e') {
		viewz--;
	}
	if (key == 's') {
		viewy--;
	}
	if (key == 'w') {
		viewy++;
	}
}

int main(int argc, char** argv){
glutInit(&argc, argv);
//glutInitDisplayMode(GLUT_SINGLE | GLUT_RGB);
glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_STENCIL | GLUT_DEPTH); //add a stencil buffer to the window
glutInitWindowSize(800,600);
glutInitWindowPosition(100,100);
glutCreateWindow("Kelompok Rumah");
init();
glutDisplayFunc(display);
glutIdleFunc(display);
glutReshapeFunc(reshape);

glutKeyboardFunc (keyboard);
glutSpecialFunc(kibor);

glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular);
glLightfv(GL_LIGHT0, GL_POSITION, light_position);

glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
glMaterialfv(GL_FRONT, GL_SHININESS, high_shininess);
glColorMaterial(GL_FRONT, GL_DIFFUSE);

glutMainLoop();
return 0;
}
