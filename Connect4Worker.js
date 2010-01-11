/*!
 * Copyright 2010, Brandon Aaron (http://brandonaaron.net/)
 * 
 * Licensed under the MIT license: LICENSE.txt.
 */
 
/**
 * @fileOverview This file is the Web Worker definition and imports the Connect4Game.js file. It is used by Connect4.js.
 */
 
importScripts('Connect4Game.js');
var connect4;
onmessage = function(event) {
    var data   = JSON.parse(event.data),
        action = data.action,
        args   = data.args,
        ret    = { action: action };
    
    if (action === 'new') {
        connect4 = new Connect4Game(args[0]);
        ret.returnValue = connect4;
    }
    else if (connect4[action]) {
        ret.returnValue = connect4[action].apply(connect4, args);
    }
    else {
        ret.returnValue = { error: 'No method for requested action: ' + action };
    }
    
    postMessage(JSON.stringify(ret));
};