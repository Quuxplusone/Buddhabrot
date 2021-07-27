#include <windows.h>
#include <stdio.h>
#include <math.h>
#include <SDL.h>
#include <SDL_thread.h>
#include <ctime>
#include <vector>


SDL_Surface *screen;

const int screen_width = 500, screen_height = 500;
bool stop = false;

#include "sdl_stuff.cpp"
#include "random.cpp"


const int max_orbit_length = 50000;
int oob;


class complex
{
public:
	long double a,b,c,d;

	complex()
	{
		a = b = c = d = 0;
	}

	complex(long double aa,long double bb,long double cc,long double dd)
	{
		a = aa;
		b = bb;
		c = cc;
		d = dd;
	}	
};

complex orbit[max_orbit_length];
float image_buffer[screen_width][screen_height][3];
int olen=0;


/***********************************************************************/
/*                                                                     */
/* All this stuff is support code, not directly related to the         */
/* Metropolis-Hastings algorithm                                       */
/*                                                                     */
/***********************************************************************/


long double dist(complex &a, complex &b)
{
	return ((a.c-b.c)*(a.c-b.c)+(a.d-b.d)*(a.d-b.d));
}


void NewRandom(complex &c)
{
	while(1)
	{
		c = complex(0,0,random(-2,2),random(-2,2));

		if(dist(c, complex(0,0,0,0))<4)
			return;
	}
}	


void RandomRange(complex &c, long double r)
{
	while(1)
	{
		c = complex(0,0,random(-r,r),random(-r,r));

		if(dist(c, complex(0,0,0,0))<r*r)
			return;
	}
}	

void InitializeImageBuffer(void)
{
	for(int x=0;x<screen_width;x++)
		for(int y=0;y<screen_height;y++)
		{
			image_buffer[x][y][0] = image_buffer[x][y][1] = image_buffer[x][y][2] = 0;
		}
}

void Clamp(int &m)
{
	if(m<0)
		m=0;

	if(m>255)
		m=255;
}


float bias(float x, float val)
{
	return (val > 0) ? pow(x,log(val) / log(0.5)) : 0;
}

float gain(float x, float val )
{
	return 0.5 * ((x < 0.5) ? bias (2*x, 1-val): (2 - bias(2-2*x,1-val)));
}


void DrawBuffer(void)
{

	int x, y;

	long double rr=0, rg=0, rb=0;



	for(x=0;x<screen_width;x++)
		for(y=0;y<screen_height;y++)
		{
			if(image_buffer[x][y][0]>rr)
				rr = image_buffer[x][y][0];

			if(image_buffer[x][y][1]>rg)
				rg = image_buffer[x][y][1];

			if(image_buffer[x][y][2]>rb)
				rb = image_buffer[x][y][2];
		}

	Slock(screen);

	for(x=0;x<screen_width;x++)
		for(y=0;y<screen_height;y++)
		{		
			int r = gain(image_buffer[x][y][0]/rr,0.2f)*512;
			int g = gain(image_buffer[x][y][1]/rg,0.2f)*512;
			int b = gain(image_buffer[x][y][2]/rb,0.2f)*512;


			Clamp(r);
			Clamp(g);
			Clamp(b);
			DrawPixel(screen, x, y, r,g,b);		
		}

	Sulock(screen);
	SDL_Flip(screen);
}





/***********************************************************************/
/*                                                                     */
/* The image coordinates and magnification for the image to be         */
/* generated.                                                          */
/*                                                                     */
/***********************************************************************/

long double zx = -1.25275, zy = -0.343, zoom = 250;

//long double zx = -0.1592, zy = -1.0317, zoom = 80.5;

//long double zx = -0.529854097, zy = -0.667968575, zoom = 80.5;

//long double zx = -0.657560793, zy = 0.467732884, zoom = 70.5;

//long double zx = -1.185768799, zy = 0.302592593, zoom = 90.5;

//ng double zx = 0.443108035, zy = 0.345012263, zoom = 4000;

//long double zx = -0.647663050, zy = 0.380700836, zoom = 1275;

/***********************************************************************/
/*                                                                     */
/* Classic run-of-the-mill Mandelbrot code                             */
/*                                                                     */
/***********************************************************************/

bool Evaluate(complex &s, int it)
{
	olen = 0;

	long double	a = s.a,
			b = s.b,
			t;

	oob = it;
	for(int i=0;i<it;i++)
	{
		if(stop)
			break;

		t=a*a - b*b + s.c;
		b=2.0*a*b + s.d;
		a=t;

		if(a*a + b*b>4.0)
			return true;
	
		orbit[olen].a = a;
		orbit[olen].b = b;
		orbit[olen].c = s.c;
		orbit[olen].d = s.d;

		olen++;

		if(olen>=max_orbit_length)
			return false;
	}

	return false;
}


/***********************************************************************/
/*                                                                     */
/* This could be replaced with a 3d projection, the algorithm should   */
/* work equally well for the classic Buddhabrot and the 4d Buddhagram  */
/*                                                                     */
/***********************************************************************/

void Project(complex &s, int &x, int &y)
{
	x =  (((s.a-zx)*zoom)+0.5)*screen_width;
	y =  (((s.b-zy)*zoom)+0.5)*screen_height;
}


void DrawOrbit(int c, long double g, complex *d_orbit, int len)
{
	for(int i=0;i<len;i++)
	{		int x,y;
		Project(d_orbit[i], x, y);

		if(x>=0&&x<screen_width && y>=0 && y<screen_height)
			image_buffer[x][y][c]+=g;

	}

}

/***********************************************************************/
/*                                                                     */
/* The contribution of each proposed mutation is proportional to the   */
/* number of times points in its orbit pass through the screen         */
/*                                                                     */
/***********************************************************************/

long double Contrib(long double q)
{
	long double contrib = 0;
	int inside=0,i;

	for(i=0;i<olen;i++)
	{
		int x,y;
		Project(orbit[i], x, y);

		if(x>=0&&x<screen_width && y>=0 && y<screen_height)
			contrib++;
	}		

	return contrib/long double(olen);
}

long double TransitionProbability(long double q1, long double q2, long double olen1, long double olen2)
{
	return (1.f-(q1-olen1)/q1)/(1.f-(q2-olen2)/q2);
}


/***********************************************************************/
/*                                                                     */
/* Returns a proposed mutation. Most of the mutations should be small, */
/* but the occasional completely new point should be thrown in to      */
/* maintain ergodicity.                                                */
/*                                                                     */
/***********************************************************************/
complex Mutate(complex &c)
{

	if(random(0,5)<4)
	{
		complex n = c;

		long double r1 = (1.f/zoom)*0.0001;
		long double r2 = (1.f/zoom)*0.1;
		long double phi = random(0,1)*2.f*3.1415926f;
		long double r = r2* exp( -log(r2/r1) * random(0,1));

		n.c += r*cos(phi);
		n.d += r*sin(phi);

		return n;
	}
	else
	{
		complex n = c;

		NewRandom(n);
		return n;
	}
	
}


/***********************************************************************/
/*                                                                     */
/* Recursively find a point whose orbit passes through the screen.     */
/* Not required per-se, but it's  alot faster than just trying random  */
/* samples over and over again until you find a good one, especially   */
/* at higher zooms.                                                    */
/*                                                                     */
/***********************************************************************/

bool FindInitialSample(complex &c, long double x, long double y, long double rad, int f)
{

	if(stop)
		return false;

	if(f>500)
	{
		return false;
	}

	complex ct = c, tmp, seed;

	int m=-1,i;
	long double closest = 1e20;

	for(i=0;i<200;i++)
	{
		if(stop)
			return false;

		RandomRange(tmp,rad);
		tmp.c += x;
		tmp.d += y;

		if(!Evaluate(tmp,50000))
			continue;

		if(Contrib(50000)>0.0f)
		{
			c = tmp;
			return true;
		}


		for(int q=0;q<olen;q++)
		{
			if(stop)
				return false;

			long double d = dist(orbit[q],complex(0,0,zx,zy));

			if(d<closest)
				m = q,
				closest = d,
				seed = tmp;
		}
	}

	return FindInitialSample(c, seed.c, seed.d, rad/2.f,f+1);
}



/***********************************************************************/
/*                                                                     */
/* For the sake of comparison, this is the old, "stupid" buddhabrot    */
/* algorithm.                                                          */
/*                                                                     */
/***********************************************************************/

void BuddhaClassic(void)
{
	int m=0;

	while(!stop)
	{
		complex z;
		
		RandomRange(z,2);

		if(Evaluate(z,50000))
		{
			DrawOrbit(0,1,orbit,olen);

			if(m%6000==0)
			{
				DrawBuffer();	
				if(!stop)
				{
					char new_title[1024];
					sprintf(new_title,"Plotted %i orbits....");
					SDL_WM_SetCaption(new_title, NULL);
				}
			}
		}

		m++;
	}
}

/***********************************************************************/
/*                                                                     */
/* Start metrobrot                                                     */
/*                                                                     */ 
/***********************************************************************/


/***********************************************************************/
/*                                                                     */
/* As many disperate part of the Mandelbrot set can produce orbits     */
/* that pass through the screen, an optimization is to run multiple    */
/* copies of the algorithm from different starting values, that way    */
/* you're less likely to miss important orbits early on.               */
/*                                                                     */ 
/***********************************************************************/
const int metro_threads = 30;


int accepted=0, rejected=0;

std::vector<complex> z_samples;
std::vector<long double> c_samples;
long double l[metro_threads][3], o[metro_threads][3];
complex n_sample,c_sample;
long double c_contrib, n_contrib;



void BuildInitialSamplePoints(void)
{
	for(int i=0;i<metro_threads;i++)
	{
		if(stop)
			break;

		l[i][0] = o[i][0] = l[i][1] = o[i][1] = l[i][2] = o[i][2] = 1;
 
		if(!stop)
		{
			char new_title[1024];
			sprintf(new_title,"Finding init sample %i",i);
			SDL_WM_SetCaption(new_title, NULL);
		}

		complex m;
		if(!FindInitialSample(m, 0, 0, 2.f, 0))
		{
			printf("Couldn't find seed %i!\n",i);
			continue;
		}

		Evaluate(m,50000);
		z_samples.push_back(m);
		c_samples.push_back(Contrib(50000));
	}
}


/***********************************************************************/
/*                                                                     */
/* In orber to avoid any bias that the initial sample points may       */
/* have, we should run the algorithm for a few thousand iterations     */
/* so that it "forgets".                                               */
/*                                                                     */ 
/***********************************************************************/



void WarmUp(void)
{
	for(int ss=0;ss<z_samples.size();ss++)
	{
		if(stop)
			break;

		if(!stop)
		{
			char new_title[1024];
			sprintf(new_title,"Warming up init sample %i of %i",ss,z_samples.size());
			SDL_WM_SetCaption(new_title, NULL);
		}

		for(int e=0;e<10000;e++)
		{

			for(int i=0;i<3;i++)
			{
				int it_length = 50*pow(10,i+1);

				if(stop)
					break;
		
				n_sample = Mutate(z_samples[ss]);
				
				if(!Evaluate(n_sample,i*pow(10,it_length)))
					continue;

				n_contrib = Contrib(it_length);

				if(n_contrib==0)
					continue;


				long double T1 = TransitionProbability(it_length, l[ss][i], olen, o[ss][i]);
				long double T2 = TransitionProbability(l[ss][i], it_length,  o[ss][i], olen);

				long double alpha = min(1.f,exp(log(n_contrib*T1)-log(c_samples[ss]*T2)));
				long double R = random01();

				if(alpha>R)
				{
					c_samples[ss] = n_contrib;
					z_samples[ss] = n_sample;

					l[ss][i] = it_length;
					o[ss][i] = olen;
				}	
			}
		}
	}
}

/***********************************************************************/
/*                                                                     */
/* Finally, the good bits.                                             */
/*                                                                     */ 
/***********************************************************************/

void RenderBuddhabrot(void)
{
	int m=0;
	while(!stop)
	{
		for(int ss=0;ss<z_samples.size();ss++)
		{
			for(int i=0;i<3;i++)
			{
				int it_length = 50*pow(10,i+1);

				
				
				n_sample = Mutate(z_samples[ss]);		


				if(!Evaluate(n_sample,it_length))
					continue;

				n_contrib = Contrib(it_length);

				if(n_contrib==0)
					continue;


				long double T1 = TransitionProbability(it_length, l[ss][i], olen, o[ss][i]);
				long double T2 = TransitionProbability(l[ss][i], it_length,  o[ss][i], olen);

				long double alpha = min(1.f,exp(log(n_contrib*T1)-log(c_samples[ss]*T2)));
				long double R = random01();

				

				if(alpha>R)
				{
					c_samples[ss] = n_contrib;
					z_samples[ss] = n_sample;

					l[ss][i] = it_length;
					o[ss][i] = olen;
					accepted++;
				
					DrawOrbit(2-i,1,orbit,olen);
				}
				else
				{
					rejected++;
				}

				m++;
				//Draw the image to the screen once in a while.
				if(m%6000==0)
				{
					DrawBuffer();	
					if(!stop)
					{
						char new_title[1024];
						sprintf(new_title,"Plotted %i orbits (a:%i r:%i)....",m,accepted,rejected);
						SDL_WM_SetCaption(new_title, NULL);
					}
				}				
			}				
		}		
	}
}

int RenderThread(void *zuh)
{	
	bool inside = true;

	//Find the inital sample points
	BuildInitialSamplePoints();


	if(c_samples.size()==0) //Just in case
	{
		printf("Oh noes!\n");
		return 0;
	}


	WarmUp();

	//Finally, start mutatin' away and plot us some fractal.
	RenderBuddhabrot();
	return 0;
}




int main(int argc, char *argv[])
{
	if ( SDL_Init(SDL_INIT_AUDIO|SDL_INIT_VIDEO) < 0 )
	{
		MessageBox(NULL,"Unable to init SDL",SDL_GetError(),MB_OK);
		SDL_Quit();
		return 1;
	}


	init_genrand((unsigned)time(NULL));
	InitializeImageBuffer();
 

	screen = SDL_SetVideoMode(screen_width,screen_height,32,SDL_HWSURFACE|SDL_DOUBLEBUF);
	
	if ( screen == NULL )
	{
		MessageBox(NULL,"Unable to init video", SDL_GetError(),MB_OK);
		SDL_Quit();
		return 1;
	}


	SDL_Thread *render_thread;

    render_thread = SDL_CreateThread(RenderThread, NULL);

	if ( render_thread == NULL )
	{
       
        SDL_Quit();
		return  1;
    }

	while(!stop)
	{
		SDL_Event event;

		while ( SDL_PollEvent(&event) )
		{
			if ( event.type == SDL_QUIT )
			{
				stop = true;
				break;
			}
		}	
	}

	SDL_WaitThread(render_thread, NULL);

	SDL_Quit();
	return 0;
}

