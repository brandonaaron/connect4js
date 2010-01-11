/**
 * @fileOverview A sample implementation of using the Connect4.js game. This sample plays the computer against itself.
 */
new Connect4({ ai: 5 }, function(game) {
    game.subscribe('moveend', drawMove);
    game.subscribe('moveend', makeNextMove);
    game.subscribe('gameend', gameOver);
    if ('console' in window) {
        game.subscribe('moveend', logMove);
        // game.subscribe('debug', debug);
    }

    // make first move
    game.autoMove(0);


    function drawMove(player, col, row) {
        player = player + 1;
        var actualRow = 6 - row,
            actualCol = col + 1,
            selector  = '#board tr:nth-of-type(' + actualRow + ') td:nth-of-type(' + actualCol + ')',
            element   = document.querySelector(selector);
        element.className = 'player' + player;
    }

    function logMove(player, col, row) {
        player = player + 1;
        console.log('Player ' + player + ' went in column ' + col + ' and row ' + row + '. ' + game.numberOfPieces + ' total pieces');

        var board  = game.board,
            width  = game.columns,
            height = game.rows,
            output = '';

        for (var y = height-1; y >= 0; y--) {
            output += '|';

            for (var x = 0; x < width; x++) {
                if (board[x][y] === -1) {
                    output += '   |';
                } else {
                    output += ' ' + board[x][y] + ' |';
                }
            }

            output += '\n';

            output += '+';
            for (var x = 0; x < width; x++) {
                output += '---+';
            }

            output += '\n';
        }

        console.log(output + '\n');
    }

    function makeNextMove(player, col, row) {
        var otherPlayer = player ^ 1;
        player = player + 1;
        if (!game.gameOver) {
            game.autoMove(otherPlayer);
        }
    }

    function gameOver() {
        document.getElementById('board').className = 'gameover';
        var log = 'console' in window;
        if (game.tie) {
            var msg = 'The game was a tie.';
            log && console.log(msg);
            alert(msg);
        } else {
            // highlight the winning pieces
            var coords = game.winningCoords;
            for (var i=0; i<coords.length; i++) {
                var row = 6 - coords[i][1],
                    col = coords[i][0] + 1;
                var el = document.querySelector('#board tr:nth-of-type(' + row + ') td:nth-of-type(' + col + ')');
                el.className = el.className + ' win';
            }
            var msg = 'Player ' + (game.winner+1) + ' won the game!';
            log && console.log(msg, coords);
            alert(msg);
        }
    }

    function debug(obj) {
        console.log(obj.message.action, obj.message);
    }
});