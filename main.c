#include "raylib.h"
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define GRID_SIZE 20
#define GRID_WIDTH (WINDOW_WIDTH / GRID_SIZE)
#define GRID_HEIGHT (WINDOW_HEIGHT / GRID_SIZE)
#define INITIAL_CAPACITY 10
#define INITIAL_APPLES 1

#define da_append(array, item)                                                 \
  do {                                                                         \
    if ((array)->count >= (array)->capacity) {                                 \
      (array)->capacity =                                                      \
          (array)->capacity == 0 ? INITIAL_CAPACITY : (array)->capacity * 2;   \
      (array)->items = realloc((array)->items,                                 \
                               (array)->capacity * sizeof(*(array)->items));   \
      assert((array)->items != NULL && "Buy more RAM lol");                    \
    }                                                                          \
    (array)->items[(array)->count++] = (item);                                 \
  } while (0)

typedef struct {
  int x;
  int y;
} IVector2;

typedef struct {
  IVector2 *items;
  int capacity;
  int count;
} List;

typedef struct {
  bool has_apple;
  bool has_body;
  bool has_snake;
} Cell;

typedef enum { RIGHT, LEFT, UP, DOWN } Direction;
typedef enum { EASIEST, EASY, MEDIUM, HARD, HARDEST } Difficulty;

typedef struct {
  IVector2 head;
  List body;
  IVector2 direction;
} Snake;

typedef struct {
  Cell grid[GRID_HEIGHT][GRID_WIDTH];
  Snake snake;
  List apples;
  float game_speed;
  int score;
} GameState;

// -----------------------------------------------------------
// Drawing the grid lines
// -----------------------------------------------------------
void draw_grid(void) {
  for (int v = 0; v <= WINDOW_WIDTH; v += GRID_SIZE) {
    DrawLine(v, 0, v, WINDOW_HEIGHT, LIGHTGRAY);
  }
  for (int h = 0; h <= WINDOW_HEIGHT; h += GRID_SIZE) {
    DrawLine(0, h, WINDOW_WIDTH, h, LIGHTGRAY);
  }
  for (int i = 0; i < GRID_WIDTH; i++) {
    int x = i * GRID_SIZE;
    for (int j = 0; j < GRID_HEIGHT; j++) {
      int y = j * GRID_SIZE;
      if ((i + j) % 2 == 0) {
        DrawRectangle(x, y, GRID_SIZE, GRID_SIZE, (Color){172, 206, 94, 200});
      } else {
        DrawRectangle(x, y, GRID_SIZE, GRID_SIZE, (Color){114, 183, 106, 200});
      }
    }
  }
}

// -----------------------------------------------------------
// Initialize game state
// -----------------------------------------------------------
void init_game_state(GameState *gs) {
  gs->snake.body.items = NULL;
  gs->snake.body.capacity = 0;
  gs->snake.body.count = 0;

  gs->apples.items = NULL;
  gs->apples.capacity = 0;
  gs->apples.count = 0;
  gs->score = 0;

  gs->game_speed = 0.2f;

  for (int y = 0; y < GRID_HEIGHT; y++) {
    for (int x = 0; x < GRID_WIDTH; x++) {
      gs->grid[y][x].has_apple = false;
      gs->grid[y][x].has_body = false;
      gs->grid[y][x].has_snake = false;
    }
  }

  IVector2 snake_head = {
      .x = GetRandomValue(0, GRID_WIDTH - 1),
      .y = GetRandomValue(0, GRID_HEIGHT - 1),
  };
  gs->snake.head = snake_head;
  gs->snake.direction = (IVector2){1, 0};
  gs->grid[snake_head.y][snake_head.x].has_snake = true;

  for (int i = 0; i < INITIAL_APPLES; i++) {
    int x, y;
    do {
      x = GetRandomValue(0, GRID_WIDTH - 1);
      y = GetRandomValue(0, GRID_HEIGHT - 1);
    } while (gs->grid[y][x].has_snake || gs->grid[y][x].has_apple);
    gs->grid[y][x].has_apple = true;

    // Store apple in grid coordinates
    IVector2 apple = {.x = x, .y = y};
    da_append(&gs->apples, apple);
  }
}

// -----------------------------------------------------------
// Check if snake head is on an apple
// -----------------------------------------------------------
bool snake_eats_apple(GameState *gs) {
  IVector2 head = gs->snake.head;
  for (int i = 0; i < gs->apples.count; i++) {
    IVector2 apple = gs->apples.items[i];
    if (head.x == apple.x && head.y == apple.y) {
      return true;
    }
  }
  return false;
}

// -----------------------------------------------------------
// Remove the apple that the snake just ate
// -----------------------------------------------------------
void remove_apple_at_head(GameState *gs) {
  IVector2 head = gs->snake.head; // grid coords
  for (int i = 0; i < gs->apples.count; i++) {
    IVector2 apple = gs->apples.items[i];
    if (apple.x == head.x && apple.y == head.y) {
      gs->apples.items[i] = gs->apples.items[gs->apples.count - 1];
      gs->apples.count--;

      gs->grid[head.y][head.x].has_apple = false;
      return;
    }
  }
}

// -----------------------------------------------------------
// Grow the snake by adding a new segment at the tail
// -----------------------------------------------------------
void grow_snake(GameState *gs) {
  if (gs->snake.body.count == 0) {
    // If no body, we add one segment behind the head
    IVector2 newTail = gs->snake.head;
    newTail.x -= gs->snake.direction.x;
    newTail.y -= gs->snake.direction.y;
    // wrap around if needed...
    da_append(&gs->snake.body, newTail);
    gs->grid[newTail.y][newTail.x].has_body = true;
  } else {
    // If there's already body segments,
    // clone the last segment (the tail)
    IVector2 tail = gs->snake.body.items[gs->snake.body.count - 1];
    da_append(&gs->snake.body, tail);
    gs->grid[tail.y][tail.x].has_body = true;
  }
}

// -----------------------------------------------------------
// Checks snake collision
// -----------------------------------------------------------
bool snake_collides_with_body(const GameState *gs) {
  for (int i = 0; i < gs->snake.body.count; i++) {
    IVector2 segment = gs->snake.body.items[i];
    if (segment.x == gs->snake.head.x && segment.y == gs->snake.head.y) {
      return true;
    }
  }
  return false;
}

// -----------------------------------------------------------
// Reset game when the snake collides with itself
// -----------------------------------------------------------
void reset_game(GameState *gs, float *time_since_last_move) {
  if (time_since_last_move != NULL) {
    *time_since_last_move = 0.0f;
  }

  free(gs->snake.body.items);
  gs->snake.body.items = NULL;
  gs->snake.body.capacity = 0;
  gs->snake.body.count = 0;

  free(gs->apples.items);
  gs->apples.items = NULL;
  gs->apples.capacity = 0;
  gs->apples.count = 0;

  gs->score = 0;

  init_game_state(gs);
}

// -----------------------------------------------------------
// Spawn new apply every time the player eats an apple
// -----------------------------------------------------------
void spawn_random_apple(GameState *gs) {
  int x, y;
  do {
    x = GetRandomValue(0, GRID_WIDTH - 1);
    y = GetRandomValue(0, GRID_HEIGHT - 1);
  } while (gs->grid[y][x].has_snake || gs->grid[y][x].has_body ||
           gs->grid[y][x].has_apple);

  gs->grid[y][x].has_apple = true;
  IVector2 newApple = {x, y};
  da_append(&gs->apples, newApple);

  // Limit total apples to prevent overcrowding
  if (gs->apples.count > 20) {
    // Remove the oldest apple
    gs->grid[gs->apples.items[0].y][gs->apples.items[0].x].has_apple = false;
    for (int i = 0; i < gs->apples.count - 1; i++) {
      gs->apples.items[i] = gs->apples.items[i + 1];
    }
    gs->apples.count--;
  }
}

// -----------------------------------------------------------
// Move snake (head and body), handle apple consumption
// -----------------------------------------------------------
void move_snake(GameState *gs, Direction dir, float *time_since_last_move) {
  // 1) Update direction from user input
  switch (dir) {
  case RIGHT:
    gs->snake.direction = (IVector2){1, 0};
    break;
  case LEFT:
    gs->snake.direction = (IVector2){-1, 0};
    break;
  case UP:
    gs->snake.direction = (IVector2){0, -1};
    break;
  case DOWN:
    gs->snake.direction = (IVector2){0, 1};
    break;
  }

  *time_since_last_move += GetFrameTime();
  if (*time_since_last_move >= gs->game_speed) {
    *time_since_last_move = 0.0f;

    // 2) Save old head
    IVector2 oldHead = gs->snake.head;
    gs->grid[oldHead.y][oldHead.x].has_snake = false;

    // 3) Move head
    gs->snake.head.x += gs->snake.direction.x;
    gs->snake.head.y += gs->snake.direction.y;
    // Wrap around
    if (gs->snake.head.x < 0)
      gs->snake.head.x = GRID_WIDTH - 1;
    if (gs->snake.head.x >= GRID_WIDTH)
      gs->snake.head.x = 0;
    if (gs->snake.head.y < 0)
      gs->snake.head.y = GRID_HEIGHT - 1;
    if (gs->snake.head.y >= GRID_HEIGHT)
      gs->snake.head.y = 0;

    gs->grid[gs->snake.head.y][gs->snake.head.x].has_snake = true;

    // 4) Shift the body segments forward
    if (gs->snake.body.count > 0) {
      for (int i = gs->snake.body.count - 1; i > 0; i--) {
        gs->snake.body.items[i] = gs->snake.body.items[i - 1];
      }
      gs->snake.body.items[0] = oldHead;
    }

    // Mark front cell as body
    if (gs->snake.body.count > 0) {
      IVector2 front = gs->snake.body.items[0];
      gs->grid[front.y][front.x].has_body = true;
    }

    // 5) Check apple collision
    bool ateApple = snake_eats_apple(gs);
    if (ateApple) {
      grow_snake(gs);
      remove_apple_at_head(gs);
      spawn_random_apple(gs);
      gs->score += 1;
    }
    if (snake_collides_with_body(gs)) {
      reset_game(gs, time_since_last_move);
    }
  }
}

// -----------------------------------------------------------
// Draw the board (apples, snake head, snake body)
// -----------------------------------------------------------
void draw_board(GameState *gs) {
  const Color SNAKE_HEAD_COLOR =
      (Color){140, 37, 154, 255};                         // Tomato red for head
  const Color SNAKE_HEAD_OUTLINE = (Color){0, 0, 0, 255}; // Crimson outline

  const Color SNAKE_BODY_COLOR_INNER = (Color){90, 127, 255, 255}; // Soft blue
  const Color SNAKE_BODY_COLOR_OUTER =
      (Color){0, 0, 0, 255}; // Darker blue outline
  const Color APPLE_COLOR_INNER = (Color){231, 76, 60, 255}; // Vibrant red
  const Color APPLE_COLOR_OUTER = (Color){0, 0, 0, 255};     // Dark red outline

  // Draw apples
  for (int i = 0; i < gs->apples.count; i++) {
    IVector2 apple = gs->apples.items[i];
    int px_center = apple.x * GRID_SIZE + (GRID_SIZE / 2);
    int py_center = apple.y * GRID_SIZE + (GRID_SIZE / 2);
    DrawCircle(px_center + 2, py_center + 2, 10.0f, (Color){0, 0, 0, 50});
    DrawCircle(px_center, py_center, 12.0f, APPLE_COLOR_OUTER);
    DrawCircle(px_center, py_center, 10.0f, APPLE_COLOR_INNER);
  }

  // Draw snake head (distinctly colored and slightly larger)
  IVector2 head = gs->snake.head;
  int sx_center = head.x * GRID_SIZE + (GRID_SIZE / 2);
  int sy_center = head.y * GRID_SIZE + (GRID_SIZE / 2);
  DrawCircle(sx_center + 2, sy_center + 2, 12.0f, (Color){0, 0, 0, 50});
  DrawCircle(sx_center, sy_center, 14.0f, SNAKE_HEAD_OUTLINE);
  DrawCircle(sx_center, sy_center, 12.0f, SNAKE_HEAD_COLOR);

  // Draw snake body
  for (int i = 0; i < gs->snake.body.count; i++) {
    IVector2 body_part = gs->snake.body.items[i];
    int px_center = body_part.x * GRID_SIZE + (GRID_SIZE / 2);
    int py_center = body_part.y * GRID_SIZE + (GRID_SIZE / 2);
    DrawCircle(px_center + 2, py_center + 2, 10.0f, (Color){0, 0, 0, 50});
    DrawCircle(px_center, py_center, 12.0f, SNAKE_BODY_COLOR_OUTER);
    DrawCircle(px_center, py_center, 10.0f, SNAKE_BODY_COLOR_INNER);
  }
}

// -----------------------------------------------------------
// Compute game difficulty
// -----------------------------------------------------------
void compute_difficulty(float game_time, int score, int snake_size,
                        float *game_speed) {
  float base_speed = 0.2f;
  float time_multiplier = 1.0f + (game_time * 0.01f);
  float score_multiplier = 1.0f + (score * 0.02f);
  float size_multiplier = 1.0f + (snake_size * 0.05f);
  float difficulty_multiplier =
      time_multiplier * score_multiplier * size_multiplier;
  float new_speed = base_speed / difficulty_multiplier;

  const float MIN_SPEED = 0.05f;
  const float MAX_SPEED = 1.0f;

  new_speed = fmaxf(MIN_SPEED, fminf(new_speed, MAX_SPEED));

  *game_speed = new_speed;
}

// -----------------------------------------------------------
// Main
// -----------------------------------------------------------
int main(void) {
  SetRandomSeed(time(NULL));
  InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Snake Game");
  SetTargetFPS(60);

  float time_since_last_move = 0.0f;
  static float apple_spawn_timer = 0.0f;
  static int last_key_pressed = -1;
  float game_time = 0.0f;

  // Score
  char scoreText[32];
  int fontSize = 20;

  GameState gs = {0};
  init_game_state(&gs);

  while (!WindowShouldClose()) {
    BeginDrawing();
    ClearBackground(RAYWHITE);
    apple_spawn_timer += GetFrameTime();
    game_time += GetFrameTime();

    // Inputs
    if (IsKeyDown(KEY_RIGHT)) {
      move_snake(&gs, RIGHT, &time_since_last_move);
      last_key_pressed = KEY_RIGHT;
    } else if (IsKeyDown(KEY_LEFT)) {
      move_snake(&gs, LEFT, &time_since_last_move);
      last_key_pressed = KEY_LEFT;
    } else if (IsKeyDown(KEY_UP)) {
      move_snake(&gs, UP, &time_since_last_move);
      last_key_pressed = UP;
    } else if (IsKeyDown(KEY_DOWN)) {
      move_snake(&gs, DOWN, &time_since_last_move);
      last_key_pressed = DOWN;
    } else {
      if (last_key_pressed != -1)
        move_snake(&gs, last_key_pressed, &time_since_last_move);
    }

    draw_grid();
    draw_board(&gs);

    static float difficulty_timer = 0.0f;
    difficulty_timer += GetFrameTime();

    if (difficulty_timer >= 5.0f) {
      compute_difficulty(game_time, gs.score, gs.snake.body.count + 1,
                         &gs.game_speed);
      difficulty_timer = 0.0f;
    }
    snprintf(scoreText, sizeof(scoreText), "Score %d", gs.score);
    DrawText(scoreText, GRID_WIDTH / 2, 15, fontSize, BLACK);
    EndDrawing();
  }

  CloseWindow();
  return 0;
}
