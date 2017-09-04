
'use strict';

const Observer = require('bindings')('testtools-observer');

Object.defineProperty(Observer, 'System', { configurable: false, enumerable: true, value: 0 });
Object.defineProperty(Observer, 'ProcessId', { configurable: false, enumerable: true, value: 1 });
Object.defineProperty(Observer, 'ProcessName', { configurable: false, enumerable: true, value: 2 });

Observer.prototype.type = function type() { return this._type(); }
Observer.prototype.object = function object() { return this._object(); }

Observer.prototype.poll = function poll(mask) {
    return new Promise((resolve, reject) => {
        if ('number' !== typeof mask)
            reject(new Error('Observer#poll - "mask" is not a number.'));

        this._poll(mask, (error, result) => {
            error === null ? resolve(result) : reject(error);
        });
    });
}

Observer.processes = function processes() {
    return new Promise((resolve, reject) => {
        Observer._processes((error, result) => {
            null === error ? resolve(result) : reject(error);
        });
    });
}

Observer.masks = function masks() {
    return {
        system: [
            { mask: 1, key: 'processes', title: '' },
            { mask: 2, key: 'threads', title: '' },
            { mask: 4, key: 'procusage', title: '' },
            { mask: 8, key: 'pmemusage', title: ''},
            { mask: 16, key: 'pmemusagekb', title: '' },
            { mask: 32, key: 'vmemusage', title: '' },
            { mask: 64, key: 'vmemusagekb', title: '' },
            { mask: 128, key: 'diskusage', title: '' }
        ],
        process: [
            { mask: 1, key: 'handles', title: '' },
            { mask: 2, key: 'threads', title: '' },
            { mask: 4, key: 'procusage', title: '' },
            { mask: 8, key: 'pmemusage', title: ''},
            { mask: 16, key: 'pmemusagekb', title: '' },
            { mask: 32, key: 'vmemusage', title: '' },
            { mask: 64, key: 'vmemusagekb', title: '' },
        ]
    }
}

module.exports = exports = Observer;
