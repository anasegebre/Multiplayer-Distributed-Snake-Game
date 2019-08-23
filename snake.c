#include <curses.h>
#include <pthread.h>
#include "scheduler.h"
#include "socket.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include "util.h"

// Defines used to track the snake direction
#define DIR_NORTH 0
#define DIR_EAST 1
#define DIR_SOUTH 2
#define DIR_WEST 3

// Game parameters
#define INIT_snake_LENGTH 4
#define snake_HORIZONTAL_INTERVAL 200
#define snake_VERTICAL_INTERVAL 300
#define DRAW_BOARD_INTERVAL 33
#define APPLE_UPDATE_INTERVAL 120
#define READ_INPUT_INTERVAL 150
#define GENERATE_APPLE_INTERVAL 2000
#define BOARD_WIDTH 50
#define BOARD_HEIGHT 25

// Game pair colors
#define SNAKE_CHAR 'O'
#define SNAKE1_PAIR 1
#define SNAKE2_PAIR 2
#define TEXT_PAIR 3
#define APPLE_PAIR 4
#define BORDER_PAIR 6
#define EMPTY_PAIR 7

/**
 * In-memory representation of the game board
 * Zero represents an empty cell
 * Positive numbers represent snake cells (which count up at each time step until they reach snake_length)
 * Negative numbers represent apple cells (which count up at each time step)
 */
int board[BOARD_HEIGHT][BOARD_WIDTH];

// snake parameters
int snake1_dir = DIR_NORTH;
int snake2_dir = DIR_NORTH;
int snake1_length = INIT_snake_LENGTH;
int snake2_length = INIT_snake_LENGTH;
int snake1_row;
int snake1_col;
int snake2_row;
int snake2_col;
int snake1_score = 0;
int snake2_score = 0;

// Client and server socket file descriptors
int client_socket_fd;
int server_socket_fd;
int socket_fd;
int end = 0;

// Apple parameters
int apple_age = 120;

// Is the game running?
bool running = true;


/*
 * Helper function for reading from sockets. If it doesn't read
 * the amount of bytes requested, it saves read information to 
 * a buffer and reads again the rest of the bytes.
 */
int read_better(int fd, void* buffer, size_t bytes) {
  int rc = read(fd, buffer, bytes);
  if(rc <= 0) return -1;
  else if(bytes - rc == 0) return 1;
  else return read_better(fd, buffer+rc, bytes - rc);
}

/*
 * Server continuously reads the direction of the snake2
 * which changes with player 2's input.
 */
void* receive_dir_thrd(void* p) {
  while(running) {
    if(read_better(client_socket_fd, &snake2_dir, sizeof(int)) == -1) {
      perror("Failed to read snake2_dir\n");
      exit(2);
    }
  }
  return NULL;
}

/*
 * Thread to continuously read the board from the server. If the thread fails to
 * read, then we know the game has ended so we set running = false and ungetch
 * to end other tasks.
 */
void* receive_board_thrd(void* p) {
  while(running) {
    if(read_better(socket_fd, &board, sizeof(int) * BOARD_HEIGHT * BOARD_WIDTH) <= 0) {
      running = false;
      ungetch(0);
    }
  }
  return NULL;
}

/**
 * Convert a board row number to a screen position
 * \param   row   The board row number to convert
 * \return        A corresponding row number for the ncurses screen
 */
int screen_row(int row) {
  return 2 + row;
}

/**
 * Convert a board column number to a screen position
 * \param   col   The board column number to convert
 * \return        A corresponding column number for the ncurses screen
 */
int screen_col(int col) {
  return 2 + col;
}

/**
 * Print the rules of the game
 */
void print_rules() {
  fprintf(stdout, "\nMultiplayer Snake Rules!\n\n");
  fprintf(stdout, "Usage for Player 1: ./snake\n");
  fprintf(stdout, "Usage for Player 2: ./snake <Player 1's Machine Name> <port number>\n\n");
  fprintf(stdout, "The player with the longest snake wins!\n\n");
  fprintf(stdout, "Don't forget that:\n");
  fprintf(stdout, "Eat the apples to become longer (before your opponent does!)\n");
  fprintf(stdout, "If you collide with the board edges, the game is over.\n");
  fprintf(stdout, "If you collide with your opponent or viceversa, the game is over.\n");
  fprintf(stdout, "Your oponent might want to make you collide if you have a shorter snake.\n");
  fprintf(stdout, "When the game is over, the player with the longest snake wins!\n");
}

/**
 * Initialize the board display by printing the title and edges
 */
void init_display() {
  // Print Title Line
  move(screen_row(-2), screen_col(BOARD_WIDTH/2 - 5));
  attron(COLOR_PAIR(TEXT_PAIR));
  addch(ACS_DIAMOND);
  addch(ACS_DIAMOND);
  printw(" Snake! ");
  addch(ACS_DIAMOND);
  addch(ACS_DIAMOND);
  attroff(COLOR_PAIR(TEXT_PAIR));

  // Print corners
  attron(COLOR_PAIR(BORDER_PAIR));
  mvaddch(screen_row(-1), screen_col(-1), ACS_ULCORNER);
  mvaddch(screen_row(-1), screen_col(BOARD_WIDTH), ACS_URCORNER);
  mvaddch(screen_row(BOARD_HEIGHT), screen_col(-1), ACS_LLCORNER);
  mvaddch(screen_row(BOARD_HEIGHT), screen_col(BOARD_WIDTH), ACS_LRCORNER);

  // Print top and bottom edges
  for(int col=0; col<BOARD_WIDTH; col++) {
    mvaddch(screen_row(-1), screen_col(col), ACS_HLINE);
    mvaddch(screen_row(BOARD_HEIGHT), screen_col(col), ACS_HLINE);
  }

  // Print left and right edges
  for(int row=0; row<BOARD_HEIGHT; row++) {
    mvaddch(screen_row(row), screen_col(-1), ACS_VLINE);
    mvaddch(screen_row(row), screen_col(BOARD_WIDTH), ACS_VLINE);
  }

  attroff(COLOR_PAIR(BORDER_PAIR));
  // Refresh the display
  refresh();
}

/*
 * Determine the scores and winner by comparing the lengths of the snakes and updates end and score variables accordingly.
 */
void score_counter() {
   for(int r = 0; r < BOARD_HEIGHT; r++) {
    for(int c = 0; c < BOARD_WIDTH; c++) {
      int cur = board[r][c];
      // Found snake1, find tail (largest number less than 625).
      if(cur > snake1_score && cur < 625) {
        snake1_score = cur;
      }
      // Found snake2, find tail (largest number greater than 624).
      if(cur > snake2_score && cur > 624) {
        snake2_score = cur;
      }
    }
  }
  // Remove initial length to determine score.
  snake2_score = snake2_score - 624 - INIT_snake_LENGTH;
  snake1_score = snake1_score - INIT_snake_LENGTH;


    if(end == 1) {
    snake2_score++;
    } else if(end == 2) {
    snake1_score++;
    }

  // Compare scores and determine winner.
  if(snake1_score > snake2_score) {
    end = 1 ;
  } else if (snake2_score > snake1_score) {
    end = 2;
  }
}

/**
 * Show a game over message, winner message, and wait for a key press.
 */
void end_game() {
  attron(COLOR_PAIR(TEXT_PAIR));
  mvprintw(screen_row(BOARD_HEIGHT/2)-1, screen_col(BOARD_WIDTH/2)-6, "            ");
  mvprintw(screen_row(BOARD_HEIGHT/2),   screen_col(BOARD_WIDTH/2)-6, " Game Over! ");
  mvprintw(screen_row(BOARD_HEIGHT/2)+1, screen_col(BOARD_WIDTH/2)-6, "            ");
  attroff(COLOR_PAIR(TEXT_PAIR));
  score_counter();
  if(end == 1) {
    attron(COLOR_PAIR(SNAKE1_PAIR));
    mvprintw(screen_row(BOARD_HEIGHT/2)+1, screen_col(BOARD_WIDTH/2)-6, "Player 1 Wins");
    attroff(COLOR_PAIR(SNAKE1_PAIR));
  } else if(end == 2) {
    attron(COLOR_PAIR(SNAKE2_PAIR));
    mvprintw(screen_row(BOARD_HEIGHT/2)+1, screen_col(BOARD_WIDTH/2)-6, "Player 2 Wins");
    attroff(COLOR_PAIR(SNAKE2_PAIR));
  } else {
    attron(COLOR_PAIR(TEXT_PAIR));
    mvprintw(screen_row(BOARD_HEIGHT/2)+1, screen_col(BOARD_WIDTH/2)-2, "Tie");
    attroff(COLOR_PAIR(TEXT_PAIR));
  }
  attron(COLOR_PAIR(TEXT_PAIR));
  mvprintw(screen_row(BOARD_HEIGHT/2)+3, screen_col(BOARD_WIDTH/2)-11, "Press any key to exit.");
  attroff(COLOR_PAIR(TEXT_PAIR));
  refresh();
  timeout(-1);
  task_readchar();
}

/**
 * Run in a thread to draw the current state of the game board.
 */
void draw_board() {
  while(running) {
    // Loop over cells of the game board
    int cur;
    for(int r=0; r<BOARD_HEIGHT; r++) {
      for(int c=0; c<BOARD_WIDTH; c++) {
        cur = board[r][c];
        if(cur == 0) {  // Draw blank spaces
          attron(COLOR_PAIR(EMPTY_PAIR));
          mvaddch(screen_row(r), screen_col(c), ' ');
          attron(COLOR_PAIR(EMPTY_PAIR));
        } else if(cur > 0 && cur < 625) {  // Draw snake
          attron(COLOR_PAIR(SNAKE1_PAIR));
          mvaddch(screen_row(r), screen_col(c), SNAKE_CHAR);
          attroff(COLOR_PAIR(SNAKE1_PAIR));
        } else if(cur >= 625) {
          attron(COLOR_PAIR(SNAKE2_PAIR));
          mvaddch(screen_row(r), screen_col(c), SNAKE_CHAR);
          attroff(COLOR_PAIR(SNAKE2_PAIR));
        } else {  // Draw apple spinner character
          char spinner_chars[] = {'|', '/', '-', '\\'};
          attron(COLOR_PAIR(APPLE_PAIR));
          mvaddch(screen_row(r), screen_col(c), spinner_chars[abs(cur % 4)]);
          attroff(COLOR_PAIR(APPLE_PAIR));
        }
      }
    }

    // Draw the scores for player 1 and player 2 (score color corresponding to snake color)
    score_counter();
    attron(COLOR_PAIR(TEXT_PAIR));
    mvprintw(screen_row(-2), screen_col(-1), " P1 Score:\r");
    mvprintw(screen_row(-2), screen_col(BOARD_WIDTH-18), "     P2 Score:\r");
    attroff(COLOR_PAIR(TEXT_PAIR));
    attron(COLOR_PAIR(SNAKE1_PAIR));
    mvprintw(screen_row(-2), screen_col(9), " %03d       \r", snake1_score);
    attroff(COLOR_PAIR(SNAKE1_PAIR));
    attron(COLOR_PAIR(SNAKE2_PAIR));
    mvprintw(screen_row(-2), screen_col(BOARD_WIDTH-4), " %03d \r", snake2_score);
    attron(COLOR_PAIR(SNAKE2_PAIR));

    // Refresh the display
    refresh();

    // Sleep for a while before drawing the board again
    task_sleep(DRAW_BOARD_INTERVAL);
  }
}

/**
 * Run in a thread to process player 1 input.
 */
void read_input1() {
  while(running) {
    // Read a character, potentially blocking this thread until a key is pressed
    int key = task_readchar();

    // Make sure the input was read correctly
    if(key == ERR) {
      running = false;
      fprintf(stderr, "ERROR READING INPUT\n");
    }

    // Handle the key press
    if(key == KEY_UP && snake1_dir != DIR_SOUTH) {
      snake1_dir = DIR_NORTH;
    } else if(key == KEY_RIGHT && snake1_dir != DIR_WEST) {
      snake1_dir = DIR_EAST;
    } else if(key == KEY_DOWN && snake1_dir != DIR_NORTH) {
      snake1_dir = DIR_SOUTH;
    } else if(key == KEY_LEFT && snake1_dir != DIR_EAST) {
      snake1_dir = DIR_WEST;
    } else if(key == 'q') {
      running = false;
    }

  }
}

/**
 * Run in a thread to process player 2 input.
 */
void read_input2() {
  while(running) {
    // Read a character, potentially blocking this thread until a key is pressed
    int key = task_readchar();

    // Make sure the input was read correctly
    if(key == ERR) {
      running = false;
      fprintf(stderr, "ERROR READING INPUT\n");
    }

    // Handle the key press
    if(key == KEY_UP && snake2_dir != DIR_SOUTH) {
      snake2_dir = DIR_NORTH;
    } else if(key == KEY_RIGHT && snake2_dir != DIR_WEST) {
      snake2_dir = DIR_EAST;
    } else if(key == KEY_DOWN && snake2_dir != DIR_NORTH) {
      snake2_dir = DIR_SOUTH;
    } else if(key == KEY_LEFT && snake2_dir != DIR_EAST) {
      snake2_dir = DIR_WEST;
    } else if(key == 'q') {
      running = false;
    }

    // Write the direction of snake2 to the server.
    if(write(socket_fd, &snake2_dir, sizeof(int)) <= 0) {
      perror("Failed to write snake2_dir\n");
      exit(2);
    }

  }
}

/**
 * Run in a thread to move player 1's snake around on the board
 */
void update_snake1() {
  while(running) {
    // "Age" each existing segment of the snake
    for(int r=0; r<BOARD_HEIGHT; r++) {
      for(int c=0; c<BOARD_WIDTH; c++) {
        if(board[r][c] == 1) {  // Found the head of the snake. Save position
          snake1_row = r;
          snake1_col = c;
        }

        // Add 1 to the age of the snake segment
        if(board[r][c] > 0 && board[r][c] < 625) {
          board[r][c]++;

          // Remove the snake segment if it is too old
          if(board[r][c] > snake1_length) {
            board[r][c] = 0;
          }
        }
      }
    }

    // Move the snake into a new space
    if(snake1_dir == DIR_NORTH) {
      snake1_row--;
    } else if(snake1_dir == DIR_SOUTH) {
      snake1_row++;
    } else if(snake1_dir == DIR_EAST) {
      snake1_col++;
    } else if(snake1_dir == DIR_WEST) {
      snake1_col--;
    }


    // Check for edge collisions
    if(snake1_row < 0 || snake1_row >= BOARD_HEIGHT || snake1_col < 0 || snake1_col >= BOARD_WIDTH) {
      running = false;
      // Add a key to the input buffer so the read_input thread can exit
      ungetch(0);
      return;
    }

    // Check for snake collisions
    if(board[snake1_row][snake1_col] > 0) {
      running = false;
      // Add a key to the input buffer so the read_input thread can exit
      ungetch(0);
      return;
    }

    // Check for apple collisions
    if(board[snake1_row][snake1_col] < 0) {
      // snake gets longer
      snake1_length++;
    }

    // Add the snake's new position
    board[snake1_row][snake1_col] = 1;

    // Once the server board has been updated, write the board to the client.
    if(write(client_socket_fd, &board, sizeof(int) * BOARD_HEIGHT * BOARD_WIDTH) <= 0) {
      running = false;
      ungetch(0);
    }

    // Update the snake movement speed to deal with rectangular cursors
    if(snake1_dir == DIR_NORTH || snake1_dir == DIR_SOUTH) {
      task_sleep(snake_VERTICAL_INTERVAL);
    } else {
      task_sleep(snake_HORIZONTAL_INTERVAL);
    }
  }
}

/**
 * Run in a thread to move player 1's snake around on the board
 */
void update_snake2() {
  while(running) {
    // "Age" each existing segment of the snake
    for(int r=0; r<BOARD_HEIGHT; r++) {
      for(int c=0; c<BOARD_WIDTH; c++) {
        if(board[r][c] == 625) {  // Found the head of the snake. Save position
          snake2_row = r;
          snake2_col = c;
        }

        // Add 1 to the age of the snake segment
        if(board[r][c] > 624) {
          board[r][c]++;

          // Remove the snake segment if it is too old
          if((board[r][c] - 624) > snake2_length) {
            board[r][c] = 0;
          }
        }
      }
    }

    // Move the snake into a new space
    if(snake2_dir == DIR_NORTH) {
      snake2_row--;
    } else if(snake2_dir == DIR_SOUTH) {
      snake2_row++;
    } else if(snake2_dir == DIR_EAST) {
      snake2_col++;
    } else if(snake2_dir == DIR_WEST) {
      snake2_col--;
    }

    // Check for edge collisions
    if(snake2_row < 0 || snake2_row >= BOARD_HEIGHT || snake2_col < 0 || snake2_col >= BOARD_WIDTH) {
      running = false;
      // Add a key to the input buffer so the read_input thread can exit
      ungetch(0);
      return;
    }

    // Check for snake collisions
    if(board[snake2_row][snake2_col] > 0) {
      running = false;
      // Add a key to the input buffer so the read_input thread can exit
      ungetch(0);
      return;
    }

    // Check for apple collisions
    if(board[snake2_row][snake2_col] < 0) {
      // snake gets longer
      snake2_length++;
    }

    // Add the snake's new position
    board[snake2_row][snake2_col] = 625;

    // Once the server board has been updated, write the board to the client.
    if(write(client_socket_fd, &board, sizeof(int) * BOARD_HEIGHT * BOARD_WIDTH) <= 0) {
      running = false;
      ungetch(0);
    }

    // Update the snake movement speed to deal with rectangular cursors
    if(snake2_dir == DIR_NORTH || snake2_dir == DIR_SOUTH) {
      task_sleep(snake_VERTICAL_INTERVAL);
    } else {
      task_sleep(snake_HORIZONTAL_INTERVAL);
    }
  }
}

/**
 * Run in a thread to update all the apples on the board.
 */
void update_apples() {
  while(running) {
    // "Age" each apple
    for(int r=0; r<BOARD_HEIGHT; r++) {
      for(int c=0; c<BOARD_WIDTH; c++) {
        if(board[r][c] < 0) {  // Add one to each apple cell
          board[r][c]++;
        }
      }
    }
    task_sleep(APPLE_UPDATE_INTERVAL);
  }
}

/**
 * Run in a thread to generate apples on the board.
 */
void generate_apple() {
  while(running) {
    bool inserted = false;
    // Repeatedly try to insert an apple at a random empty cell
    while(!inserted) {
      int r = rand() % BOARD_HEIGHT;
      int c = rand() % BOARD_WIDTH;

      // If the cell is empty, add an apple
      if(board[r][c] == 0) {
        // Pick a random age between apple_age/2 and apple_age*1.5
        // Negative numbers represent apples, so negate the whole value
        board[r][c] = -((rand() % apple_age) + apple_age / 2);
        inserted = true;
      }
    }
    task_sleep(GENERATE_APPLE_INTERVAL);
  }
}

// Entry point: Sets up the main server, waits for client to connect, creates jobs, then runs the scheduler
int main(int argc, char** argv) {

  // Set up server
  if(argc == 1) {

    // Starting the game case
    unsigned short port = 0;
    server_socket_fd = server_socket_open(&port);
    if(server_socket_fd == -1) {
      perror("Server socket was not opened");
      exit(2);
    }

    // Start listening for connections, with a maximum of one queued connection
    if(listen(server_socket_fd, 1)) {
      perror("listen failed");
      exit(2);
    }

    // Print server's port number
    printf("Server listening on port %u\n", port);

    // Accept client's connection
    client_socket_fd = server_socket_accept(server_socket_fd);
    if(client_socket_fd == -1) {
      perror("accept failed");
      exit(2);
    }

    // Create thread to continuously read the keys of the the client
    pthread_t server_receive_dir;
    pthread_create(&server_receive_dir, NULL, receive_dir_thrd, NULL);
  }

  // Player wants to read the rules
  else if(argc == 2) {
    print_rules();
    exit(1);
  }

  // Player 2 connecting to the game case
  else if(argc == 3) {

    // Read command line arguments
    char* server_name = argv[1];
    unsigned short port = atoi(argv[2]);

    // Connect to the server
    socket_fd = socket_connect(server_name, port);
    if(socket_fd == -1) {
      perror("Failed to connect");
      exit(2);
    }

    // Create thread to continuously read the board of the the server
    pthread_t client_receive;
    pthread_create(&client_receive, NULL, receive_board_thrd, NULL);


  } else {
    fprintf(stderr, "Usage for Player 1: %s\n", argv[0]);
    fprintf(stderr, "Usage for Player 2: %s <Player 1's Machine Name> <port number>]\n", argv[0]);
    fprintf(stderr, "Usage for Rules: %s rules\n", argv[0]);
    exit(1);
  }

  // Initialize the ncurses window
  WINDOW* mainwin = initscr();
  if(mainwin == NULL) {
    fprintf(stderr, "Error initializing ncurses.\n");
    exit(2);
  }
  // Check if machine supports ncurses colors
  if (has_colors() == FALSE) {
    endwin();
    fprintf(stderr, "Your terminal does not support color.\n");
    exit(2);
  }

  // Initialize game pair colors
  start_color();
  init_pair(SNAKE1_PAIR, COLOR_BLUE, COLOR_YELLOW);
  init_pair(SNAKE2_PAIR, COLOR_MAGENTA, COLOR_YELLOW);
  init_pair(TEXT_PAIR, COLOR_BLACK, COLOR_YELLOW);
  init_pair(APPLE_PAIR, COLOR_RED, COLOR_YELLOW);
  init_pair(BORDER_PAIR, COLOR_CYAN, COLOR_YELLOW);
  init_pair(EMPTY_PAIR, COLOR_YELLOW, COLOR_YELLOW);

  // Seed random number generator with the time in milliseconds
  srand(time_ms());

  noecho();               // Don't print keys when pressed
  keypad(mainwin, true);  // Support arrow keys
  nodelay(mainwin, true); // Non-blocking keyboard access

  // Initialize the game display
  init_display();

  // Zero out the board contents
  memset(board, 0, BOARD_WIDTH*BOARD_HEIGHT*sizeof(int));

  // Put the snakes at the middle of the board
  board[BOARD_HEIGHT/2][(BOARD_WIDTH/2) - 2] = 1; // head of snake1 is 1
  board[BOARD_HEIGHT/2][(BOARD_WIDTH/2) + 2] = 625; // head of snake2 is 625 (half the area of board)

  // Thread handles for each of the game threads
  task_t update_snake1_thread = 0;
  task_t update_snake2_thread = 0;
  task_t draw_board_thread;
  task_t read_input1_thread = 0;
  task_t read_input2_thread = 0;
  task_t update_apples_thread = 0;
  task_t generate_apple_thread;

  // Initialize the scheduler library
  scheduler_init();

  if(argc == 3) {
    // Create threads for each task in the game
    task_create(&draw_board_thread, draw_board);
    task_create(&read_input2_thread, read_input2);

    // Wait for these threads to exit
    task_wait(draw_board_thread);
    task_wait(read_input2_thread);
  } else {
    // Create threads for each task in the game
    task_create(&update_snake1_thread, update_snake1);
    task_create(&update_snake2_thread, update_snake2);
    task_create(&draw_board_thread, draw_board);
    task_create(&read_input1_thread, read_input1);
    task_create(&update_apples_thread, update_apples);
    task_create(&generate_apple_thread, generate_apple);

    // Wait for these threads to exit
    task_wait(update_snake1_thread);
    task_wait(update_snake2_thread);
    task_wait(draw_board_thread);
    task_wait(read_input1_thread);
    task_wait(update_apples_thread);
  }

  // Don't wait for the generate_apple task because it sleeps for 2 seconds,
  // which creates a noticeable delay when exiting.
  //task_wait(generate_apple_thread);

  // Display the end of game message and wait for user input
  end_game();

  // Clean up window
  delwin(mainwin);
  endwin();

  return 0;
}
