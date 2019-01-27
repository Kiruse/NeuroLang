////////////////////////////////////////////////////////////////////////////////
// Unit Test main app
// -----
// Copyright (c) Kiruse 2018 Germany
// License: GPL 3.0
////////////////////////////////////////////////////////////////////////////////
const fs = require('fs').promises;
const path = require('path');
const {execFile} = require('child_process');
const conf = require('./test.conf.json');

const testpath = path.resolve(path.dirname(__filename), conf.path);
const testExePattern = new RegExp(conf.pattern);

const {RootContext, SectionContext, TestContext} = require('./context');

require('./util.string');

(async function(){
    let listings = await fs.readdir(testpath),
        root     = new RootContext(),
        ctx      = root,
        promises = [];
    
    listings.forEach(item => {
        // Skip pattern mismatches
        if (!testExePattern.test(item)) return;
        
        promises.push(new Promise(res => {
            execFile(`${testpath}/${item}`, (err, stdout, stderr) => {
                if (err) {
                    console.error(err);
                    return;
                }
                
                const lines = stdout.replace('\r', '').split('\n');
                for (let line of lines) {
                    line = line.trim();
                    if (!line.length) continue;
                    
                    ctx = ctx.processLine(line);
                }
                
                res();
            });
        }))
    });
    
    await Promise.all(promises);
    
    if (root !== ctx) throw ContextualError(`Syntax error: final section "${ctx.name}" is not root`, ctx);
    
    displayTreatedResults(root.sort());
})();

function displayTreatedResults(ctx) {
    // Alignment offset for ticks and crosses in this section
    const [numSuccessfulTests, numTotalTests] = countAllTests(ctx);
    
    displayTreatedSectionResults(ctx);
    
    if (numSuccessfulTests === numTotalTests) {
        console.log('\x1b[32mAll tests successful\x1b[0m');
    }
    else {
        console.log(`\x1b[31m${numSuccessfulTests} of ${numTotalTests} tests successful\x1b[0m`);
    }
}

function displayTreatedSectionResults(ctx, level = 0) {
    const alignOffset = getResultAlignOffset(ctx, level + 1);
    const [numSuccessfulTests, numTotalTests] = countSectionTests(ctx);
    
    console.log();
    
    let msg   = `${ctx.name.indent(level)} (${numSuccessfulTests}/${numTotalTests})`;
    let color = '\x1b[0m';
    
    if (numSuccessfulTests !== numTotalTests) {
        color = '\x1b[31m';
    }
    else {
        color = '\x1b[32m';
    }
    
    console.log(`${color}${msg}\x1b[0m`);
    
    getSubsections(ctx).forEach(sub => {
        displayTreatedSectionResults(sub, level + 1);
    })
    
    getTests(ctx).forEach(test => {
        let msg = test.name.indent(level + 1).padEnd(alignOffset);
        let color = '\x1b[0m';
        
        if (test.errors.length) {
            msg += '\u00d7';
            color = '\x1b[31m';
        }
        else {
            msg += '\u2713';
            color = '\x1b[32m';
        }
        
        console.log(`${color}${msg}\x1b[0m`);
        
        if (test.errors.length) {
            for (let error of test.errors) {
                console.log(`\x1b[31m${error}\x1b[0m`.indent(level + 2));
            }
        }
    })
}

function countAllTests(section) {
    let [successful, total] = countSectionTests(section);
    let subs = getSubsections(section).map(sub => countAllTests(sub));
    return [
        successful + subs.reduce((succ,  [subsucc])    => succ  + subsucc,  0),
        total      + subs.reduce((total, [, subtotal]) => total + subtotal, 0)
    ]
}

function countSectionTests(section) {
    const tests = getTests(section);
    return [
        // Successful tests
        tests.filter(curr => !curr.errors.length).length,
        // Total tests
        tests.length
    ];
}

function getResultAlignOffset(section, level) {
    return getTests(section).reduce((length, curr) => Math.max(length, curr.name.indent(level).length), 0) + 2;
}

function getSubsections(section) {
    return section.children.filter(elem => elem instanceof SectionContext);
}

function getTests(section) {
    return section.children.filter(elem => elem instanceof TestContext);
}
