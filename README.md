# Multiplayer-Distributed-Snake-Game

This is a multiplayer snake game that can be played accross machines. 


Usage:
On your terminal, compile the game with the command:

$make


Player 1 serves as the central server, displays the port number and waits for a connection, use the command:

$./snake


Player 2 can now connect to the game with the following command:

$./snake <Player 1's Machine Name> <port number>


Multiplayer Snake Rules!
1. The player with the longest snake wins!
2. Eat the randomly generated apples to become longer (before your opponent does!)
3. If you collide with the board edges, the game is over.
4. If you collide with your opponent or viceversa, the game is over.
5. Your oponent might want to make you collide if you have a shorter snake
6. When the game is over, the player with the longest snake wins!


For a full report on this project, check this link out!
https://docs.google.com/document/d/1nRwjOhzFpjEkH0krd49ToDp8umAaxL2GCm2u8CFRgNw/edit?usp=sharing
