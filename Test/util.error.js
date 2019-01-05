////////////////////////////////////////////////////////////////////////////////
// More or less a state machine for parsing the stdout of a unit test executable.
// -----
// Copyright (c) Kiruse 2018 Germany
// License: GPL 3.0
/**
 * Creates a new or expands an existing error with a `context` property.
 * @param {string|Error} err Error instance or error message to create a new instance with.
 * @param {*} context Arbitrary data to set as context.
 * @returns {Error} The expanded error object.
 */
exports.ContextualError = (err, context) => Object.assign(typeof err === 'string' ? Error(err) : err, {context});

/**
 * Creates a new or expands an existing error with a `skippable` property. Only
 * errors that both have this property and have it set to truthy should be
 * considered skippable.
 * @param {string|Error} err Error instance or error message to create a new instance with.
 * @param {boolean} skippable Whether this error is considered skippable.
 * @returns {Error} The expanded error object.
 */
exports.SkippableError = (err, skippable) => Object.assign(typeof err === 'string' ? Error(err) : err, {skippable: !!skippable});
