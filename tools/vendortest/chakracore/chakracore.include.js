var nativePrint = print;
var nativeGlobal = this;
var WScript = {
    Echo : function() {
        var length = arguments.length;
        var finalResult = "";
        for (var i = 0; i < length; i++) {
            if (i != 0)
                finalResult += " ";
            var arg = arguments[i];
            if (typeof arg == undefined || arg == null)
                finalResult += arg;
            else
                finalResult += (arg.toString());
        }
        nativePrint(finalResult);
    },
    LoadScriptFile : function(path) {
        path = path.replace(/\\/g, "/");
        try {
            load(path);
        } catch (e) {
            if (e.message == "GlobalObject.load: cannot load file")
                load("test\\chakracore\\UnitTestFramework\\" + path);
            else
                throw e;
        }
        return nativeGlobal;
    },
    Arguments : ["summary"]
};

function CollectGarbage() {
    gc();
}
