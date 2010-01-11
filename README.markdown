# Connect4.js

This is a JavaScript implementation of a [Connect 4](http://en.wikipedia.org/wiki/Connect_Four) Algorithm based on the original work of Keith Pomakis in C. This project was a way for me to do some exploring that I've wanted to do for sometime now. First, I wanted to learn more about game development ([zero-sum](http://en.wikipedia.org/wiki/Zero-sum), [minimax](http://en.wikipedia.org/wiki/Minimax), [alpha-beta pruning](http://en.wikipedia.org/wiki/Alpha-beta_pruning)). Second, I wanted to explore Web Workers and see how the modern browsers handle a game AI like Connect 4 written entirely in JavaScript.

# Files

There are 3 main files that make up the game.

* Connect4.js - This is the primary file that you would include in your HTML and provides the public API.
* Connect4Game.js - This is the game AI/logic and is only used via the Web Worker.
* Connect4Worker.js - This is the definition of the Web Worker and it imports the Connect4Game.js file.

# Demo

The game.html and game.js provide an example of how to use the Connect4.js game. Just open game.html in a modern browser that supports Web Workers and open the console. The compute will play against itself. Just reload to restart the game. I also have the demo up and running on my own site here: [http://brandonaaron.net/code/connect4js/demos](http://brandonaaron.net/code/connect4js/demos)

# API/Docs

The API is super simple and uses a pubsub strategy to deal with the asynchronous behavior of Web Workers. Although the documentation isn't great there is inline documentation in the JSDoc-Toolkit format.

# License

This work is Copyright 2010 [Brandon Aaron](http://brandonaaron.net/) and licensed under the MIT license (LICENSE.txt).

## Original Algorithm

I've very happy that Keith Pomakis put his work out in Public Domain for others to learn and build upon. I've included his original C implementation and released my port under MIT with his permission. You can find the original work that I used in the folder named 'originalAlgorithm'.