/*!
 * Copyright 2010, Brandon Aaron (http://brandonaaron.net/)
 * 
 * Licensed under the MIT license: LICENSE.txt.
 */
 
/**
 * @fileOverview The actual public API for the Connect 4 game. It uses Web Workers to offload the heavy computations.
 * This is the only JS file that needs to be included but make sure that Connect4Worker.js and Connect4Game.js are
 * in the same directory.
 */

/**
 * Creates a new Connect 4 game asynchronously using Web Workers. A simple pubsub system is used to manage
 * the asynchronous behavior. The valid events are: gamestart, movestart, moveend, gameend, and debug. Each
 * event receives different arguments. The gamestart event receives only the instance of the Connect 4 game.
 * The movestart event receives the player (0 for player 1, 1 for player 2) that is moving and the instance 
 * of the Connect 4 game. The moveend event receives the player (0 for player 1, 1 for player 2), the column, the row,
 * and the instance of the Connect 4 game. The gameend event receives only the instance of the Connect 4 game.
 * The debug event receives a debug object.
 *
 * @param {object} [settings] The settings for the game.
 * @param {number} [settings.cols = 7] The number of columns.
 * @param {number} [settings.rows = 6] The number of rows.
 * @param {number} [settings.connect = 4] The number of pieces to connect to win.
 * @param {number} [settings.ai = 4] The default level to walk the tree of possible moves.
 * @param {function} [callback] The callback to be called once the game is ready.
 * @param {object} [context = Connect4 instance] The context for the callback.
 *
 * @property {boolean} moveInProgress True if a move is in progress, false otherwise.
 * @property {boolean} gameOver True if the game is over, false otherwise.
 * @property {number} winner The winner of the game, -1 if no winner. Player 1 is 0 and Player 2 is 1.
 * @property {boolean} tie True if the game is over and resulted in a tie, false otherwise.
 * @property {array} board The board configuration of the game state. board[x][y] specifies the 
 *                         position of the xth column and the yth row of the board, where column and 
 *                         row numbering starts at 0. (The 0th row is the bottom row.) A value of 0 
 *                         specifies that the position is occupied by a piece owned by player 1, a 
 *                         value of 1 specifies that the position is occupied by a piece owned by 
 *                         player 2, and -1 specifies that the position is unoccupied.
 * @property {array} stats An array specifying stats on both players. stats[0] specifies the 
 *                         stats for player 1, while stats[1] specifies the stats for player 2.
 * @property {array} map An array in which each element is a list specifying, for each corresponding 
 *                       board space, which n-in-a-row areas it is part of.
 * @property {array} score The actual scores of each player, deducible from score_array, but kept 
 *                         separately for efficiency. The score of player x is the sum of score[x]. 
 *                         A score is basically a function of how many winning positions are still 
 *                         available to the and how close he/she is to achieving each of 
 *                         these positions.
 * @property {number} columns The number of columns.
 * @property {number} rows The number of rows.
 * @property {number} connect The number of pieces to connect to win.
 * @property {number} numberOfPieces The total number of pieces currently played.
 * @property {array} winningCoords The winning coordinates.
 */
function Connect4(settings, callback, context) {
    if (typeof settings === 'function') {
        context  = callback;
        callback = settings;
        settings = {};
    }

    this._subscribers = {};
    this.moveInProgress = false;
    this.gameOver = false;
    this.winner = -1;
    this.tie = false;
    this.board = [];
    this.stats = [[],[]],
    this.map = [],
    this.score = [],
    this.columns = 0;
    this.rows = 0;
    this.connect = 0;
    this.numberOfPieces = 0;
    this.winningCoords = [];

    var self = this;
    this._worker = new Worker('Connect4Worker.js');
    this._worker.onmessage = function() { return self._onmessage.apply(self, arguments); };

    this._postMessage('new', [settings]);
    this.subscribe('gamestart', callback, context);

    return this;
};

/**
 * Drops the game piece in a given column for the given player. Publishes the movestart event at the beginning
 * of the move and publishes the moveend event once the move is finished.
 *
 * @param {number} player The player, 0 for player 1 and 1 for player 1.
 * @param {number} column The column to drop in starting from 0.
 *
 * @throws moveInProgress If a move is currently in progress.
 * @throws gameOver If the game is over.
 */
Connect4.prototype.makeMove = function(player, column) {
    if (this.moveInProgress) {
        throw new Error('There is currently a move in progress');
    } else if (this.gameOver) {
        throw new Error('The game is over.');
    }
    
    this._moveStart(player);
    this._postMessage('makeMove', arguments);
    return this;
};

/**
 * Makes the move for the given player. Publishes the movestart event at the beginning
 * of the move and publishes the moveend event once the move is finished.
 *
 * @param {number} player The player, 0 for player 1 and 1 for player 1.
 * @param {ai} [ai = settings.ai] The level of the AI, or how deep to search the game tree.
 *
 * @throws moveInProgress If a move is currently in progress.
 * @throws gameOver If the game is over.
 */
Connect4.prototype.autoMove = function(player, ai) {
    if (this.moveInProgress) {
        throw new Error('There is currently a move in progress');
    } else if (this.gameOver) {
        throw new Error('The game is over.');
    }
    
    this._moveStart(player);
    this._postMessage('autoMove', arguments);
    return this;
};

/**
 * Subscribes to an event.
 *
 * @param {string} event The name of the event, all lowercase.
 * @param {function} handler The function to call when the event is published. Must be a named function to unsubscribe.
 * @param {object} [context = Connect4 instance] The context to call the function with.
 */
Connect4.prototype.subscribe = function(event, handler, context) {
    this._subscribers[event] = this._subscribers[event] || [];
    this._subscribers[event].push([handler, context]);
    return this;
};

/**
 * Unsubscribe from an event.
 *
 * @param {string} event The name of the event, all lowercase.
 * @param {function} handler The function to call when the event is published. Must be a named function to unsubscribe.
 * @param {object} [context = Connect4 instance] The context to call the function with.
 */
Connect4.prototype.unsubscribe = function(event, handler, context) {
    var index = this._subscribers[event].indexOf([handler, context]);
    this._subscribers[event].slice(index, 1);
};

/**
 * Internal handle of movestart event.
 * @private
 */
Connect4.prototype._moveStart = function(player) {
    this.moveInProgress = true;
    this._publish('movestart', [player, this]);
};

/**
 * Internal handle of moveend event.
 * @private
 */
Connect4.prototype._moveEnd = function(data) {
    this.moveInProgress = false;
    this._publish('moveend', [data.player, data.col, data.row, this]);
};

/**
 * Internal handle of gamestart event.
 * @private
 */
Connect4.prototype._gameStart = function(game) {
    this.columns = game._cols;
    this.rows = game._rows;
    this.connect = game._connect;
    this._updateState(game.currentState);
    this._publish('gamestart', [this]);
};

/**
 * Updates the current state of the game.
 * @private
 */
Connect4.prototype._updateState = function(currentState) {
    for (var prop in currentState) {
        if (currentState.hasOwnProperty(prop)) {
            this[prop] = currentState[prop];
        }
    }
};

/**
 * Fires an event and passes along any args.
 * @private
 */
Connect4.prototype._publish = function(event, args) {
    var index = 0, subscribers = this._subscribers[event] || [], subscriber;
    while ((subscriber = subscribers[index++])) {
        if (subscriber[0]) {
            subscriber[0].apply(subscriber[1] || this, args);
        }
    }
};

/**
 * Handle receiving a message from the worker.
 * @private
 */
Connect4.prototype._onmessage = function(event) {
    var data        = JSON.parse(event.data),
        action      = data.action,
        returnValue = data.returnValue;
    
    if (returnValue && returnValue.currentState) {
        this._updateState(returnValue.currentState);
    }
    
    switch (action) {
        case 'autoMove':
        case 'makeMove':
            this._moveEnd(returnValue);
            break;
        case 'new':
            this._gameStart(returnValue);
            break;
        case 'debug':
            this._publish('debug', [data]);
            break;
        default:
            this._publish(action, [returnValue]);
    }
    
    if (this.gameOver) {
        this._publish('gameend', [this]);
    }
};

/**
 * Send a message to the worker.
 * @private
 */
Connect4.prototype._postMessage = function(action, args) {
    this._worker.postMessage(JSON.stringify({
        action: action,
        args: [].slice.call(args)
    }));
};