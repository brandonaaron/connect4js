/*!
 * Copyright 2010, Brandon Aaron (http://brandonaaron.net/)
 *
 * Original Connect-4 Algorithm written in C is by Keith Pomakis placed in Public Domain.
 * Used and redestributed with explicit permission.
 *
 * Licensed under the MIT license: LICENSE.txt.
 */

/**
 * @fileOverview This file is imported via the Connect4Worker.js and is used by the public API in Connect4.js via
 * Web Workers to avoid locking upt he borwser during computations.
 */

/**
 * A JavaScript implementation of the Connect 4 Algorithim based on the original work done in C by Keith Pomakis.
 * Multiple board sizes are supported. It is also possible to specify the number of pieces necessary to connect
 * in a row in order to win.  Therefore one can play Connect-3, Connect-5, etc.  An efficient tree-searching
 * algorithm (making use of alpha-beta cutoff decisions) has been implemented to insure that the computer plays
 * as quickly as possible
 *
 * @property {object} currentState The current state of the game. Includes the following properties:
 *                                 numberOfPieces: The number of pieces currently played.
 *                                 winner: The winner of the game, if there is one.
 *                                 tie: True if the game is tied.
 *                                 gameOver: True if the game is over.
 *                                 board: The board configuration of the game state. board[x][y] specifies the
 *                                        position of the xth column and the yth row of the board, where column and
 *                                        row numbering starts at 0. (The 0th row is the bottom row.) A value of 0
 *                                        specifies that the position is occupied by a piece owned by player 1, a
 *                                        value of 1 specifies that the position is occupied by a piece owned by
 *                                        player 2, and -1 specifies that the position is unoccupied.
 *                                 stats: An array specifying stats on both players. stats[0] specifies the
 *                                        stats for player 1, while stats[1] specifies the stats for player 2.
 *                                 map: An array in which each element is a list specifying, for each corresponding
 *                                      board space, which n-in-a-row areas it is part of.
 *                                 winningCoords: An array of winning coordinates.
 *                                 score: The actual scores of each player, deducible from score_array, but kept
 *                                        separately for efficiency. The score of player x is the sum of score[x].
 *                                        A score is basically a function of how many winning positions are still
 *                                        available to the and how close he/she is to achieving each of
 *                                        these positions.
 */
function Connect4Game(settings) {
    // mixin the settings
    for (var key in Connect4Game.settings.defaults) {
        if (!Connect4Game.settings.defaults.hasOwnProperty(key)) { continue; }
        var setting = settings && settings[key];
        // prefix all settings with an underscore
        this['_'+key] = setting !== undefined ? setting : Connect4Game.settings.defaults[key];
    }

    this._ai = Math.max(0, Math.min(this._ai, 20)); // make sure ai level is within bounds

    this._none = -1;
    this._magicWinningNumber = 1 << this._connect;
    this._winningPlaces = this._findWinningPlaces();

    this._stateStack = [{
        numberOfPieces: 0,
        winner: this._none,
        tie: false,
        gameOver: false,
        board: [],
        stats: [[],[]],
        map: [],
        winningCoords: [],
        score: [this._winningPlaces, this._winningPlaces]
    }];

    this.currentState = this._stateStack[0];

    this._setupBoard();
    this._setupPlayerStats();
    this._setupMap();
    this._setupDropOrder();

    return this;
}

/**
 * The default settings for the game.
 */
Connect4Game.settings = {
    defaults: {
        cols: 7,
        rows: 6,
        connect: 4,
        ai: 4
    }
};

/**
 * This function returns the "score" of the specified player. This score is a function of how many winning
 * positions are still available to the player and how close he/she is to achieving each of these positions.
 * The scores of both players can be compared to observe how well they are doing relative to each other.
 *
 * @param {number} player The player, 0 for player 1 and 1 for player 1.
 *
 * @returns {number} The score for the requested player.
 */
Connect4Game.prototype.scoreOfPlayer = function(player) {
    return this.currentState.score[player];
};

/**
 * Checks to see if a particular player is the winner.
 *
 * @param {number} player The player, 0 for player 1 and 1 for player 1.
 *
 * @returns {boolean} True if the player is the winner, false otherwise.
 */
Connect4Game.prototype.isWinner = function(player) {
    return this.currentState.winner === player;
};

/**
 * Checks to see if the game is a tie.
 *
 * @returns {boolean} True if the game is over and resulted in a tie, false otherwise.
 */
Connect4Game.prototype.isTie = function() {
    return this.currentState.winner === this._none && this.currentState.numberOfPieces >= this._cols * this._rows;
};

/**
 * Drops the game piece in a given column for the given player.
 *
 * @param {number} player The player, 0 for player 1 and 1 for player 1.
 * @param {number} column The column to drop in starting from 0.
 *
 * @returns {object} An object with the following properties:
 *                   col: the column the piece was dropped in,
 *                   row: the row the piece ended up in,
 *                   currentState: the currentState of the board.
 *
 * @throws An error if the column is not valid.
 */
Connect4Game.prototype.makeMove = function(player, column) {
    this._debug({
        action: 'makeMove',
        player: player,
        column: column,
        'this': this
    });

    if (column >= this._cols || column < 0) {
        throw new Error('Not a valid column.');
    }

    var ret = {
        col: column,
        row: this._dropPiece(player, column),
        player: player,
        currentState: this.currentState
    };

    if (this.currentState.gameOver && !this.currentState.tie) {
        this.currentState.winningCoords = this.winningCoords();
    }

    return ret;
};

/**
 * Makes the best possible move for the given player assuming the other player makes the best possible moves.
 *
 * @param {number} player The player, 0 for player 1 and 1 for player 1.
 * @param {number} ai The level of the AI, or how deep to search the game tree.
 */
Connect4Game.prototype.autoMove = function(player, ai) {
    var bestColumn = -1,
        goodness   = 0,
        bestWorst  = -(Number.MAX_VALUE);

    ai = Math.max(0, Math.min((ai !== undefined ? ai : this._ai), 20));

    /* It has been proven that the best first move for a standard 7x6 game  */
    /* of connect-4 is the center column.  See Victor Allis' masters thesis */
    /* ("ftp://ftp.cs.vu.nl/pub/victor/connect4.ps") for this proof.        */

    if (this.currentState.numberOfPieces < 2 && this._cols === 7 && this._rows === 6 && this._connect === 4 &&
            (this.currentState.numberOfPieces === 0 || this.currentState.board[3][0] !== 2)) {
        return this.makeMove(player, 3);
    }

    // Simulate a drop in each of the columns and see what the results are

    for (var i=0; i<this._cols; i++) {
        this._pushState();

        var column = this._dropOrder[i];

        var row = this._dropPiece(player, column);
        // if this column is full, ignore it as a possiblity
        if (row < 0) {
            this._popState();
            continue;
        }
        // if this drop wins the game, take it
        else if (this.currentState.winner === player) {
            bestColumn = column;

            this._debug({
                action: 'autoMove',
                checkColumn: column,
                reason: 'wins',
                bestColumn: bestColumn
            });

            this._popState();
            break;
        }
        // Otherwise, look ahead to see how good this move may turn out
        // assuming the opponent makes the best moves possible
        else {
            goodness = this._evaluate(player, ai, -(Number.MAX_VALUE), -(bestWorst));
        }

        // if this move looks better than the ones previously considered, remember it
        if (goodness > bestWorst) {
            bestWorst = goodness;
            bestColumn = column;

            this._debug({
                action: 'autoMove',
                checkColumn: column,
                reason: 'better',
                bestColumn: bestColumn,
                goodness: goodness,
                bestWorst: bestWorst
            });
        }
        // if two moves are equally as good, make a random decision
        else if (goodness === bestWorst) {
            // BUG: sometimes the only column left to go in is still the worst
            // and in this case will pick a random column between -1 and the given column...
            // should find a better fix for this than just the check for -1 value for bestColumn
            if (bestColumn === -1 || Math.floor(Math.random()*2) > 0) {
                var prevBestColumn = bestColumn;
                bestColumn = column;

                this._debug({
                    action: 'autoMove',
                    checkColumn: column,
                    reason: 'random',
                    bestColumn: bestColumn,
                    prevBestColumn: prevBestColumn,
                    goodness: goodness,
                    bestWorst: bestWorst
                });
            }
        }

        this._popState();
    }

    // make the move
    return this.makeMove(player, bestColumn);
};

/**
 * Gets the coordinates of the winning pieces.
 *
 * @returns {Array} The winning coordinates
 */
Connect4Game.prototype.winningCoords = function() {
    var winPos = 0,
        coords = [];

    while (this.currentState.stats[this.currentState.winner][winPos] !== this._magicWinningNumber) {
        winPos++;
    }

    for (var column=0; column<this._cols; column++) {
        for (var row=0; row<this._rows; row++) {
            var map1, map2;
            if ((map1 = this.currentState.map[column]) !== undefined && (map2 = map1[row]) !== undefined) {
                for (var i=0, l=map2.length; i<l; i++) {
                    if (map2[i] === winPos) {
                        coords.push([column,row]);
                    }
                }
            }
        }
    }

    return coords;
};

/**
 * This function drops a piece of the specified player into the specified column. The row where the piece ended
 * up is returned, or -1 if the drop was unsuccessful (i.e., the specified column is full).
 * @private
 *
 * @returns {number} The row where the piece ended up or -1 if the drop was unsuccessful.
 */
Connect4Game.prototype._dropPiece = function(player, column) {
    var row = 0,
        col = this.currentState.board[column];

    while (col[row] !== this._none && (++row < this._rows));

    if (row === this._rows) {
        return -1;
    }

    this.currentState.board[column][row] = player;
    this.currentState.numberOfPieces++;
    this._updateScore(player, column, row);

    return row;
};

/**
 * This function updates the score of the specified player in the context of the current state, given that the
 * player has just placed a game piece in column x, row y.
 * @private
 *
 * @param {number} player The player, 0 for player 1 and 1 for player 1.
 * @param {number} column The column starting at 0.
 * @param {number} row The row starting at 0.
 */
Connect4Game.prototype._updateScore = function(player, column, row) {
    var thisDiff    = 0,
        otherDiff   = 0,
        otherPlayer = this._other(player),
        map         = this.currentState.map[column][row],
        stats       = this.currentState.stats,
        length      = map.length;

    for (var i=0; i < length; i++) {
        var winIndex = map[i];
        thisDiff  += stats[player][winIndex];
        otherDiff += stats[otherPlayer][winIndex];

        stats[player][winIndex] <<= 1;
        stats[otherPlayer][winIndex] = 0;

        if (stats[player][winIndex] == this._magicWinningNumber) {
            if (this.currentState.winner == this._none) {
                this.currentState.winner = player;
            }
        }
    }

    this.currentState.score[player] += thisDiff;
    this.currentState.score[otherPlayer] -= otherDiff;

    if (this.currentState.winner > -1 || this.isTie()) {
        this.currentState.gameOver = true;
        this.currentState.tie = this.isTie();
    }
};

/**
 * This recursive function determines how good the current state may turn out to be for the specified player.
 * It does this by looking ahead level moves. It is assumed that both the specified player and the opponent
 * may make the best move possible. alpha and beta are used for alpha-beta cutoff so that the game tree can be
 * pruned to avoid searching unneccessary paths.
 *
 * The worst goodness that the current state can produce in the number of moves (levels) searched is returned.
 * This is the best the specified player can hope to achieve with this state (since it is assumed that the
 * opponent will make the best moves possible).
 * @private
 */
Connect4Game.prototype._evaluate = function(player, level, alpha, beta) {
    var goodness, best, maxab, depth = this._stateStack.length;

    if (level === depth) {
        return this._goodnessOf(player);
    }
    else {
        /* Assume it is the other player's turn. */
        best = -(Number.MAX_VALUE);
        maxab = alpha;

        for (var i=0; i<this._cols; i++) {
            this._pushState();

            if (this._dropPiece(this._other(player), this._dropOrder[i]) < 0) {
                this._popState();
                continue;
            }
            else if (this.currentState.winner === this._other(player)) {
                goodness = Number.MAX_VALUE - depth;
            }
            else {
                goodness = this._evaluate(this._other(player), level, -beta, -maxab);
            }

            if (goodness > best) {
                best = goodness;
                if (best > maxab) {
                    maxab = best;
                }
            }

            this._popState();
            if (best > beta) {
                break;
            }
        }

        // What's good for the other player is bad for this one
        return -best;
    }
};

/**
 * Initialize the game board with empty moves.
 * @private
 */
Connect4Game.prototype._setupBoard = function() {
    for (var i=0; i<this._cols; i++) {
        this.currentState.board[i] = [];
        for (var j=0; j<this._rows; j++) {
            this.currentState.board[i][j] = this._none;
        }
    }
};

/**
 * Initialize the player stats.
 * @private
 */
Connect4Game.prototype._setupPlayerStats = function() {
    for (var i=0; i<this._winningPlaces; i++) {
        this.currentState.stats[0][i] = 1;
        this.currentState.stats[1][i] = 1;
    }
};

/**
 * Initialize the map with the winning positions.
 * @private
 */
Connect4Game.prototype._setupMap = function() {
    for (var i=0; i<this._cols; i++) {
        this.currentState.map[i] = [];
        for (var j=0; j<this._rows; j++) {
            this.currentState.map[i][j] = [];
        }
    }

    var winIndex = 0;

    // fill in the horizontal win positions
    for (var i=0; i<this._rows; i++) {
        for (var j=0; j<this._cols-this._connect+1; j++) {
            for (var k=0; k<this._connect; k++) {
                var winIndices = this.currentState.map[j+k][i];
                if (this.currentState.map[j+k][i] === undefined) {
                    this.currentState.map[j+k][i] = [];
                }
                this.currentState.map[j+k][i].push(winIndex);
            }
            winIndex++;
        }
    }

    // fill in the vertical win positions
    for (var i=0; i<this._cols; i++) {
        for (var j=0; j<this._rows-this._connect+1; j++) {
            for (var k=0; k<this._connect; k++) {
                if (this.currentState.map[i][j+k] === undefined) {
                    this.currentState.map[i][j+k] = [];
                }
                this.currentState.map[i][j+k].push(winIndex);
            }
            winIndex++;
        }
    }

    // fill in the forward diagonal win positions
    for (var i=0; i<this._rows-this._connect+1; i++) {
        for (var j=0; j<this._cols-this._connect+1; j++) {
            for (var k=0; k<this._connect; k++) {
                if (this.currentState.map[j+k][i+k] === undefined) {
                    this.currentState.map[j+k][i+k] = [];
                }
                this.currentState.map[j+k][i+k].push(winIndex);
            }
            winIndex++;
        }
    }

    // fill in the backward diagonal win positions
    for (var i=0; i<this._rows-this._connect+1; i++) {
        for (var j=this._cols-1; j>=this._connect-1; j--) {
            for (var k=0; k<this._connect; k++) {
                if (this.currentState.map[j-k][i+k] === undefined) {
                    this.currentState.map[j-k][i+k] = [];
                }
                this.currentState.map[j-k][i+k].push(winIndex);
            }
            winIndex++;
        }
    }
};

/**
 * Initialize the particular drop order for the autoMove method.
 * @private
 */
Connect4Game.prototype._setupDropOrder = function() {
    // setup the order in which automatic moves should be tried
    // the columns nearer to the center of the board are usually
    // better tactically and are more likely to lead to a win.
    // by ordering the search such that the centeral columns are
    // tried frist, alpha-beta cutoff is much more effective
    this._dropOrder = [];
    var column = (this._cols-1) / 2;
    for (var i=1; i<=this._cols; i++) {
        this._dropOrder[i-1] = column;
        column += ((i%2) ? i : -i);
    }
};

/**
 * Push a new state of the game onto the stack.
 * @private
 */
Connect4Game.prototype._pushState = function() {
    this._stateStack.push(this._clone(this._stateStack[this._stateStack.length - 1]));
    this.currentState = this._stateStack[this._stateStack.length-1];
};

/**
 * Revert to the previous version of the state.
 * @private
 */
Connect4Game.prototype._popState = function() {
    this._stateStack.pop();
    this.currentState = this._stateStack[this._stateStack.length-1];
};

/**
 * Helper method to clone Array's and Objects.
 * @private
 */
Connect4Game.prototype._clone = function(obj) {
    var type   = Object.prototype.toString.call(obj),
        cloned = obj;

    if (type === '[object Array]') {
        cloned = [];
        for (var index = 0, length = obj.length; index < length; index++) {
            cloned[index] = this._clone(obj[index]);
        }
    }
    else if (type === '[object Object]') {
        cloned = {};
        for (var key in obj) {
            obj.hasOwnProperty(key) && (cloned[key] = this._clone(obj[key]));
        }
    }

    return cloned;
};

/**
 * Return the other player.
 * @private
 */
Connect4Game.prototype._other = function(player) {
    return player ^ 1;
};

/**
 * The "goodness" of the current state with respect to a player is the score of that player minus the score of the
 * player's opponent. A positive value will result if the specified player is in a better situation than
 * his/her opponent.
 * @private
 */
Connect4Game.prototype._goodnessOf = function(player) {
    return (this.currentState.score[player] - this.currentState.score[this._other(player)]);
};

/**
 * Sends a given message back to the main interface.
 * @private
 */
Connect4Game.prototype._debug = function(message) {
    postMessage(JSON.stringify({
        action: 'debug',
        message: message
    }));
};

/**
 * This function returns the number of possible win positions on a board of dimensions x by y with n being the
 * number of pieces required in a row in order to win.
 * @private
 */
Connect4Game.prototype._findWinningPlaces = function() {
    var x = this._cols,
        y = this._rows,
        n = this._connect;

    if (x < n && y < n) {
        return 0;
    }

    if (x < n) {
        return x * ((y - n) + 1);
    }

    if (y < n) {
        return y * ((x - n) + 1);
    }

    return 4*x*y - 3*x*n - 3*y*n + 3*x + 3*y - 4*n + 2*n*n + 2;
};