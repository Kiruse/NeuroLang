////////////////////////////////////////////////////////////////////////////////
// More or less a state machine for parsing the stdout of a unit test executable.
// 
// -----
// Basic Principle
// -----
// Contexts are basically more or less dynamic states. The `processLine` method
// is the heart of the state machine. It iterates through the `_handlers`
// property and invokes every single function through `handler.call(this, line)`.
// 
// Handlers are expected to return falsey if the current line is not applicable,
// or the next state if applicable.
// 
// Handlers may throw.
// 
// -----
// Copyright (c) Kiruse 2018 Germany
// License: GPL 3.0
const {ContextualError} = require('./util.error');
require('./util.string')

const UnexpectedCommandError = (currentState, command) => Error(`Unexpected command "${command}"`, {currentState, command});

const Context = exports.Context = class {
    constructor(name, parent) {
        if (parent && !(parent instanceof Context)) throw ContextualError('Invalid parent: not a Context object', parent);
        
        this.name      = name + '';
        this.parent    = parent;
        this.children  = [];
        this._handlers = [];
    }
    
    processLine(line) {
        let result = undefined;
        
        for (let handler of this._handlers) {
            if (typeof handler === 'function') {
                result = handler.call(this, line);
                if (result) break;
            }
        }
        
        if (!result) throw UnexpectedCommandError(this.name, line);
        
        return result;
    }
    
    enterSubSection(name) {
        let ctx = new SectionContext(name, this);
        this.children.push(ctx);
        return ctx;
    }
    
    enterTest(name) {
        let ctx = new TestContext(name, this);
        this.children.push(ctx);
        return ctx;
    }
    
    /**
     * Sorts this context's children and all their children by their names.
     * @chainable
     */
    sort() {
        this.children.sort((a, b) => {
            a.sort();
            b.sort();
            
            if (a instanceof TestContext && b instanceof SectionContext) return  1;
            if (a instanceof SectionContext && b instanceof TestContext) return -1;
            
            return a.name.localeCompare(b.name);
        })
        
        return this;
    }
}

exports.RootContext = class extends Context {
    constructor() {
        super('root');
        
        this._handlers = [
            this.processEnterSection,
            this.processEnterTest,
        ]
    }
    
    processEnterSection(line) {
        let matches = line.match(/^enter section (.*)$/);
        if (!matches) return false;
        
        return this.enterSubSection(matches[1].trim());
    }
    
    processEnterTest(line) {
        let matches = line.match(/^enter test (.*)$/);
        if (!matches) return false;
        
        return this.enterTest(matches[1].trim());
    }
}

const SectionContext = exports.SectionContext = class extends Context {
    constructor(name, parent) {
        super(name, parent);
        
        this._handlers = [
            this.processEnterSection,
            this.processLeaveSection,
            this.processEnterTest,
        ]
    }
    
    processEnterSection(line) {
        let matches = line.match(/^enter section (.*)$/);
        if (!matches) return false;
        
        return this.enterSubSection(matches[1].trim());
    }
    
    processLeaveSection(line) {
        let matches = line.match(/^leave section (.*)$/);
        if (!matches) return false;
        
        const name = matches[1].trim();
        
        if (name !== this.name) throw ContextualError(`Invalid leave section ${name}, expected ${this.name}`);
        
        return this.parent;
    }
    
    processEnterTest(line) {
        let matches = line.match(/^enter test (.*)$/);
        if (!matches) return false;
        
        return this.enterTest(matches[1].trim());
    }
}

const TestContext = exports.TestContext = class extends Context {
    constructor(name, parent) {
        super(name, parent);
        
        this._handlers = [
            this.processLeaveTest,
            this.processError,
        ]
        
        this.errors = [];
        
        Object.defineProperty(this, 'success', {
            enumerable: true,
            configurable: true,
            get() {
                return !this.errors.length;
            },
        });
    }
    
    processLeaveTest(line) {
        let matches = line.match(/^leave test (.*)$/);
        if (!matches) return false;
        
        const name = matches[1].trim();
        
        if (name !== this.name) throw ContextualError(`Invalid leave test ${name}, expected ${this.name}`);
        
        return this.parent;
    }
    
    processError(line) {
        let matches = line.match(/^error (.*)$/);
        if (!matches) return false;
        
        this.errors.push(matches[1].trim());
        return this;
    }
}
