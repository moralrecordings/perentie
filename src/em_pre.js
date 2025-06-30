Module['preRun'] = [
    function() {
        FS.createPreloadedFile('/', 'data.pt', 'data.pt', true, false);
        FS.mkdir('/libsdl');
        FS.mount(IDBFS, {autoPersist: true}, '/libsdl');
    },
];
