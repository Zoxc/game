#include <SDL.h> 
#include <cairo.h> 
#include <assert.h>
#include <Eigen/Dense>
#include <algorithm> 

typedef Eigen::Vector2f Vector;

const size_t width = 800;
const size_t height = 480;

const size_t pixels_per_meter = 64;

const size_t ground_height = height / 5;

class Sprite
{
	public:
		cairo_surface_t *surface;
		Vector offset;
	
		Sprite(std::string filename);
		~Sprite();

		void render(cairo_t *cr, Vector position);
};

Sprite::Sprite(std::string filename)
{
	surface = cairo_image_surface_create_from_png(filename.c_str());

	offset << (float)(0 - cairo_image_surface_get_width(surface) / 2), (float)cairo_image_surface_get_height(surface);
}

Sprite::~Sprite()
{
	cairo_surface_destroy(surface);
}

void Sprite::render(cairo_t *cr, Vector position)
{
	position += offset;

	cairo_set_source_surface(cr, surface, position.x(), height - position.y());
	cairo_paint(cr);
}

class Entity
{
	public:
		int direction;
		Sprite *sprite;
		Sprite *sprite_running;
		Vector position;
		Vector velocity;
		
		Entity(Sprite *sprite, Sprite *sprite_running);

		void render(cairo_t *cr);
		void step(float time, float input, float jump);
};

Entity::Entity(Sprite *sprite, Sprite *sprite_running) : direction(1), sprite(sprite), sprite_running(sprite_running)
{
	position << 0, 0;
	velocity << 0, 0;
}

void Entity::render(cairo_t *cr)
{
	cairo_save(cr);
	cairo_scale(cr, direction, 1.0);

	Sprite *to_draw = abs(velocity[0]) > 10.0f ? sprite_running : sprite;
	
	to_draw->render(cr, Vector(position[0] * direction, position[1])); 

	cairo_restore(cr);
}

void Entity::step(float time, float input, float jump)
{
	// Ensure we cannot jump in the air and reduce movement speed
	
	if(position[1] != (float)ground_height)
	{
		jump = 0.0f;
		input *= 0.5;
	}

	float gravity = -9.81f * pixels_per_meter;
	
	// Step X and Y axis seperatly

	velocity[0] += (input - 8 * velocity[0]) * time;

	velocity[1] += jump + (gravity - 0.2f * velocity[1]) * time;

	position += velocity * time;
	
	if(velocity[0] != 0.0f)
		direction = velocity[0] > 0.0f ? 1 : -1;

	// Snap to ground plane
	if(position[1] < (float)ground_height)
	{
		position[1] = ground_height;
		velocity[1] = 0;
	}
}

int character_input = 0;
int character_jump = 0;

bool handle_event(SDL_Event &event)
{
	switch (event.type) {
		case SDL_KEYUP:
			{
				switch(event.key.keysym.sym){
					case SDLK_a:
					case SDLK_LEFT:
						character_input += 1;
						break;

					case SDLK_d:
					case SDLK_RIGHT:
						character_input -= 1;
						break;
				}
			}
			break;

		case SDL_KEYDOWN:
			{
				switch(event.key.keysym.sym){
					case SDLK_a:
					case SDLK_LEFT:
						character_input -= 1;
						break;

					case SDLK_d:
					case SDLK_RIGHT:
						character_input += 1;
						break;
						
					case SDLK_w:
					case SDLK_UP:
					case SDLK_SPACE:
						character_jump = 1;
						break;
				}
			}
			break;

		case SDL_QUIT:
			return false;
	}

	return true;
}

cairo_pattern_t *sky_pattern;
cairo_pattern_t *ground_pattern;
Sprite *guy;
Sprite *guy_running;

Entity *character;

void allocate()
{
	character_input << 1, 0;

	sky_pattern = cairo_pattern_create_linear(0, 0, 0, height);
	
	cairo_pattern_add_color_stop_rgb(sky_pattern, 0.0, 114 / 255.0, 177 / 255.0, 211 / 255.0);
	cairo_pattern_add_color_stop_rgb(sky_pattern, 1.0, 221 / 255.0, 236 / 255.0, 244 / 255.0);
	
	ground_pattern = cairo_pattern_create_linear(0, 0, 0, height);

	cairo_pattern_add_color_stop_rgb(ground_pattern, 0.0, 170 / 255.0, 140 / 255.0, 72 / 255.0);
	cairo_pattern_add_color_stop_rgb(ground_pattern, 1.0, 102 / 255.0, 91 / 255.0, 68 / 255.0);
	
	guy = new Sprite("guy.png");
	guy_running = new Sprite("guy-running.png");

	character = new Entity(guy, guy_running);
	character->position(0) += 50;
	character->position(1) += ground_height;
}

void deallocate()
{
	delete character;

	delete guy;
	delete guy_running;

	cairo_pattern_destroy(sky_pattern);
	cairo_pattern_destroy(ground_pattern);
}

void step(float time)
{
	character->step(time, character_input * 1500.0f, character_jump * 350.0f);

	character_jump = 0;	
}

void render(cairo_t *cr)
{
	cairo_set_source(cr, sky_pattern);
	cairo_paint(cr);

	cairo_set_source(cr, ground_pattern);
	cairo_rectangle(cr, 0, height - ground_height, width, ground_height);
	cairo_fill(cr);

	character->render(cr);

	assert(cairo_status(cr) == CAIRO_STATUS_SUCCESS);
}

cairo_surface_t *cairo_surface_from_sdl_surface(SDL_Surface *surface)
{
	cairo_surface_t *result = cairo_image_surface_create_for_data((unsigned char *)surface->pixels, CAIRO_FORMAT_RGB24, surface->w, surface->h, surface->pitch);
	return result;
}

int main(int argc, char *argv[])
{
	SDL_Init(SDL_INIT_EVERYTHING);

	SDL_Surface *screen = SDL_SetVideoMode(width, height, 32, SDL_SWSURFACE | SDL_DOUBLEBUF);
	
	assert(screen && "Unable to set video mode");

	SDL_Surface *frame = SDL_CreateRGBSurface(SDL_SWSURFACE, width, height, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0);
	
	allocate();

	SDL_Event event;

	bool running = true;

	size_t time = SDL_GetTicks();

	while(running)
	{
		while(running && SDL_PollEvent(&event))
		{
			running = handle_event(event);
		}

		size_t new_time = SDL_GetTicks();

		size_t frame_time = new_time - time;

		time = new_time;

		step(frame_time / 1000.0f);
		
		SDL_LockSurface(frame);

		cairo_surface_t *surface = cairo_surface_from_sdl_surface(frame);
		cairo_t *cr = cairo_create(surface);

		render(cr);

		cairo_destroy(cr);
		cairo_surface_destroy(surface);

		SDL_UnlockSurface(frame);

		SDL_BlitSurface(frame, 0, screen, 0);

		SDL_Flip(screen);
	}

	deallocate();

	SDL_FreeSurface(frame);

    SDL_Quit();
	return 0;
}
