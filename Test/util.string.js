////////////////////////////////////////////////////////////////////////////////
// Utility functions for strings.
// -----
// Copyright (c) Kiruse 2018 Germany
// License: GPL 3.0
////////////////////////////////////////////////////////////////////////////////
const proto = String.prototype;

proto.startsWith = proto.startsWith || function(prefix) {
    if (this.length < prefix.length) return false;
    for (let i = 0; i < prefix.length; ++i) {
        if (this.charAt(i) !== prefix.charAt(i)) return false;
    }
    return true;
}

proto.endsWith = proto.endsWith || function(suffix) {
    if (this.length < suffix.length) return false;
    for (let i = 0; i < suffix.length; ++i) {
        if (this.charAt(this.length - suffix.length + i) !== suffix.charAt(i)) return false;
    }
    return true;
}

proto.left = proto.left || function(count) {
    return this.substr(0, left);
}

proto.right = proto.right || function(count) {
    return this.substr(this.length - count);
}

proto.trimLeft = proto.trimLeft || function(count) {
    return this.substr(count);
}

proto.trimRight = proto.trimRight || function(count) {
    return this.substr(0, this.length - count);
}

proto.indent = proto.indent || function(level, spaces = 4) {
    return (new Array(spaces * level).fill(' ').join('')) + this;
}
