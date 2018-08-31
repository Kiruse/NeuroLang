'use strict';

const child_process = require('child_process');
const fs = require('fs').promises;
const path = require('path');
const {promisify} = require('util');

const exec = promisify(child_process.exec);

////////////////////////////////////////////////////////////////////////////////
// Special promisification.

function promisedSpawn(cmd, args, opts) {
    const child = child_process.spawn(cmd, args, opts);
    return {
        process: child,
        promise: new Promise((resolve, reject) => {
            child.on('error', err => {reject(err)})
                .on('exit', code => {
                    if (code) reject(Error(`Call to "${cmd}" failed with error code ${code}`));
                    else resolve();
                });
        })
    };
}


////////////////////////////////////////////////////////////////////////////////
// Yargs configuration.

const commonOpts = {
    builddir: {
        alias: 'at',
        desc: 'Alternate build staging directory.',
        default: './build',
        type: 'string',
    },
    x86: {
        alias: '32bit',
        desc: 'Whether to build for 32 bit environments. Mutually exclusive with the x64 flag. When on a 32 bit system, the x64 flag takes precedence. When on a 64 bit system, the x86 flag takes precedence.',
        default: false,
        type: 'boolean',
    },
    x64: {
        alias: '64bit',
        desc: 'Whether to build for 64 bit environments. Mutually exclusive with the x86 flag. When on a 32 bit system, the x64 flag takes precedence. When on a 64 bit system, the x86 flag takes precedence.',
        default: false,
        type: 'boolean',
    },
};
const commonLibOpts = {
    shared: {
        alias: 's',
        desc: 'Whether to build shared libraries (default). Negate with `-no-shared`.',
        default: true,
        type: 'boolean',
    },
};

const argv = require('yargs')
    .command('*', 'Build the complete Neuro Environment.',
        yargs => yargs.options(commonOpts).options(commonLibOpts),
        buildGeneric.bind(undefined, buildAll)
    )
    .command('lang', 'Build the Neuro Language interpreter / compiler only.',
        yargs => yargs.options(commonOpts).options(commonLibOpts),
        buildGeneric.bind(undefined, buildLang)
    )
    .command('runtime', 'Build the Neuro Runtime only.',
        yargs => yargs.options(commonOpts).options(commonLibOpts),
        buildGeneric.bind(undefined, buildRT)
    )
    .command('tests', 'Build the complete Neuro Test Suite. Future versions of the build tool may feature filtering which unit tests to build.',
        yargs => yargs.options(commonOpts),
        buildGeneric.bind(undefined, buildTest)
    )
    .command('clean', 'Clean the build staging directory in preparation of a clean rebuild.',
        yargs => yargs,
        clean
    )
    .argv;


////////////////////////////////////////////////////////////////////////////////
// Build targets & helpers.

async function buildGeneric(delegate, argv) {
    try {
        console.log('Staging build...');
        await verifyBuildDir(argv.builddir);
        await updateMeta(argv);
        
        console.log('Initiating build...');
        await delegate(argv);
        console.log('Build completed.');
        
        console.log('Updating .neurobuild...');
        let meta = await getBuildMeta();
        meta.buildnumber++;
        meta.buildtime = Date.now();
        await writeBuildMeta(meta);
        
        // Prevent yargs from printing the help message...
        process.exit(0);
    }
    catch (err) {
        console.error('Build process failed due to unhandled exception:');
        console.error(err);
        process.exit(1);
    }
}

/**
 * Build all targets: NeuroLang, NeuroRT and NeuroTest.
 * @param {object} argv 
 */
async function buildAll(argv) {
    await buildVSProject(argv, 'ALL_BUILD');
}

/**
 * Only build the NeuroLang library.
 * @param {object} argv 
 */
async function buildLang(argv) {
    await buildVSProject(argv, 'NeuroLang');
}

/**
 * Only build the NeuroRT library.
 * @param {object} argv 
 */
async function buildRT(argv) {
    await buildVSProject(argv, 'NeuroRT');
}

/**
 * Build the Neuro unit tests including their dependencies (both NeuroLang and NeuroRT).
 * @param {object} argv 
 */
async function buildTest(argv) {
    await buildVSProject(argv, 'NeuroTest');
}

/**
 * Clean the build staging directory.
 * @param {object} argv 
 */
async function clean(argv) {
    await rimruf(path.resolve('./build'));
}

////////////////////////////////////////////////////////////////////////////////
// Build utility functions.

async function buildCMake(argv) {
    await promisedSpawn('cmake', ['-G', 'Visual Studio 15 2017', '..'], {
        cwd: 'build',
        stdio: 'inherit',
    }).promise;
}

async function buildVSProject(argv, project) {
    const pathToMSBuild = await findMSBuild(argv);
    const {process: proc, promise} = promisedSpawn(pathToMSBuild, [`${project}.vcxproj`], {
        cwd: path.resolve('./build'),
    });
    
    // Manually pipe stdout and stderr, so we can more easily intercept them later.
    proc.stdout.pipe(process.stdout);
    proc.stderr.pipe(process.stderr);
    
    await promise;
}


////////////////////////////////////////////////////////////////////////////////
// Locate VS-installation-dependent executables.

/**
 * Attempts to locate any installation of MSBuild on the system.
 * @async
 * @param {object} argv Additional arguments to further fine tune the search.
 * @returns {string} Path to MSBuild.
 */
async function findMSBuild(argv) {
    try {
        return await findMSBuild15(argv);
    }
    catch (err) {}
    
    try {
        return await findMSBuild14(argv);
    }
    catch (err) {}
    
    throw Error('No Visual Studio installation discovered!');
}

/**
 * Attempt to locate the VS2017 MSBuild tool using the fixed-path executable
 * "<programfiles(x86)>\Microsoft Visual Studio\Installer\vswhere.exe".
 * @async
 * @param {object} argv Additional arguments to further fine tune the search.
 * @returns {string} Path to VS2017. Throws if not found.
 */
async function findMSBuild15(argv) {
    const isWin32 = !('programfiles(x86)' in process.env);
    const programFiles = isWin32 ? process.env['programfiles'] : process.env['programfiles(x86)'];
    
    const pathToVSWhere = path.win32.join(programFiles, 'Microsoft Visual Studio', 'Installer', 'vswhere');
    const pathToVS = JSON.parse((await exec(`"${pathToVSWhere}" -format json`)).stdout)[0].installationPath;
    return path.win32.join(pathToVS, 'MSBuild', '15.0', 'Bin', isWin32 ? '' : 'amd64', 'MSBuild');
}

/**
 * Attempt to locate the VS2015 MSBuild tool through the registry.
 * @async
 * @param {object} argv Additional arguments to further fine tune the search.
 * @returns {string} Path to VS2015. Throws if not found.
 */
async function findMSBuild14(argv) {
    const hive = 'HKLM\\SOFTWARE\\Microsoft\\MSBuild\\14.0';
    const regedit = require('regedit');
    const regls = promisify(regedit.list);
    
    let reg = await regls(hive);
    return path.join(reg[hive].values.MSBuildOverrideTasksPath.value, 'MSBuild');
}


////////////////////////////////////////////////////////////////////////////////
// Build meta / stage specific functions.

/**
 * Verifies certain aspects of the specified build directory. If this method
 * returns successfully, the build directory is ready for use.
 * @param {string} builddir Path to the build directory.
 */
async function verifyBuildDir(builddir) {
    try {
        await fs.access(builddir)
    }
    catch (err) {
        await fs.mkdir(builddir);
    }
    if (!(await fs.stat(builddir)).isDirectory()) throw Error(`"${path.resolve(builddir)}" is not a directory, please manually remove it, or specify an alternative build staging location!`);
}

async function updateMeta(argv) {
    // Get existing build meta if any, or create a new one.
    let meta = await getBuildMeta();
    
    // Match file hierarchy.
    let added = getAddedSources(argv, meta);
    let removed = getRemovedSources(argv, meta);
    
    added = await added;
    removed = await removed;
    
    if (added.length || removed.length) {
        meta.sources = meta.sources
            .concat(added)
            .filter(item => removed.indexOf(item) === -1);
        
        console.log('File structure has been altered, rebuilding CMake...');
        await buildCMake();
        await writeBuildMeta(meta);
    }
    else {
        console.log('File structure unaltered.');
    }
}

async function getAddedSources(argv, meta) {
    let result = [];
    
    const body = (item) => {
        item.path = path.resolve(item.path);
        const ext = path.extname(item.path).toLowerCase();
        if (ext !== '.cpp' && ext !== '.c') return;
        
        if (meta.sources.indexOf(item.path) === -1) {
            result.push(item.path);
        }
    };
    
    for await (let item of await walk('Source', {yieldSimple: false})) {
        body(item);
    }
    for await (let item of await walk('Test', {yieldSimple: false})) {
        body(item);
    }
    
    return result;
}

async function getRemovedSources(argv, meta) {
    let result = [];
    
    for (let source of meta.sources) {
        try {
            await fs.access(source);
        }
        catch (err) {
            result.push(source);
        }
    }
    
    return result;
}

/**
 * Attempts to get the build meta data of the last build. Throws upon failure.
 * @param {string} builddir Path to the build staging directory.
 * @return {Promise<object>} A promise yielding an object holding the meta data of the last build.
 */
async function getBuildMeta() {
    try {
        await fs.access('.neurobuild');
    }
    catch (err) {
        return {
            version: '1.0',
            buildtime: 0,
            buildnumber: 0,
            sources: [],
        };
    }
    return JSON.parse(await fs.readFile('.neurobuild', 'utf8'));
}

async function writeBuildMeta(meta) {
    await fs.writeFile('.neurobuild', JSON.stringify(meta));
}


////////////////////////////////////////////////////////////////////////////////
// File system utility functions.

/**
 * Essentially `rm -rf ${target}`! (Try to read rm -rf c;)
 * The function being asynchronous makes this much faster, as we don't have to
 * wait for the file system's response for every single file before we move on
 * to request removing the next.
 * @param {string} target Path to the target file or directory to remove.
 * @async
 * @returns {Promise} Makes use of the ES6 async feature.
 */
async function rimruf(target) {
    if (!(await fs.stat(target)).isDirectory()) {
        await fs.unlink(target);
    }
    else {
        // Read all items in the target directory, map them to the non-await'ed async call, and then await all returned Promises.
        await Promise.all((await fs.readdir(target)).map(item => rimruf(path.join(target, item))));
    }
}

/**
 * Callback to filter out individual directories from the walk algorithm.
 * @callback walkFilter
 * @param {string} path Current directory to filter.
 * @returns {boolean} True if the walker should skip over this directory, otherwise false.
 */

/**
 * Walks the entire specified directory, yielding awaitable promises
 * along the way.
 * 
 * Has two modes of yield: simple and advanced. When yielding simple, each
 * resolved promise yields an entry in the entire structure. Directories are
 * suffixed with `require('path').sep`, while non-directories simply aren't.
 * When yielding advanced, the promises resolve to {path, stat, isDir},
 * containing the respective information (path to the item, fs.Stat, and
 * fs.Stat.isDirectory()).
 * 
 * The generator itself must be awaited at first to allow it to retrieve a list
 * of entries in the directory first.
 * 
 * Example usage:
 * 
 *     for (let promisedFile of await walk('foo')) {
 *         let file = await promisedFile;
 *         // ... do stuff with the file!
 *     }
 * 
 * Alternate usage:
 * 
 *     walk('foo').then(it => {
 *         for (let promisedFile of it) {
 *             promisedFile.then(file => {
 *                 // ... do stuff with the file!
 *             });
 *         }
 *     });
 * 
 * @param {string} target Target directory hierarchy to walk.
 * @param {walkFilter=} filter
 * @generator
 * @yields {Promise<string>}
 */
async function* walk(target, opts) {
    opts = opts || {};
    opts.filter = opts.filter || function() { return true; };
    if (!('yieldSimple' in opts)) opts.yieldSimple = true;
    
    // Start reading the directory entries. We'll need them later.
    for (let item of await fs.readdir(target)) {
        const curr = path.join(target, item);
        const stat = await fs.stat(curr);
        const result = {
            path: curr,
            stat: stat,
            isDir: stat.isDirectory(),
        };
        
        if (result.isDir) {
            if (opts.yieldSimple) yield curr + path.sep;
            else yield result;
            if (opts.filter(curr)) {
                yield* walk(curr, opts);
            }
        }
        else {
            if (opts.yieldSimple) yield curr;
            else yield result;
        }
    }
}
