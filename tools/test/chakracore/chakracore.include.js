var nativePrint = print;
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
    LoadScriptFile : function(path, mode) {
        path = path.replace(/\\/g, "/");
        var target = globalThis;
        if (!!mode && mode != "self") {
            target = $262.createRealm().global
            try {
                target.load("tools/test/chakracore/chakracore.include.js");
            } catch(e) {
                target.load("../../../../tools/test/chakracore/chakracore.include.js");
            }
        }
        var subPathList = [
          "test/vendortest/ChakraCore/UnitTestFramework/",
          "test/vendortest/ChakraCore/Array/",
          "test/vendortest/ChakraCore/Function/",
          "test/vendortest/ChakraCore/Object/",
          "test/vendortest/ChakraCore/Operators/",
          "test/vendortest/ChakraCore/InlineCaches/",
          "test/vendortest/ChakraCore/es5/"
        ]
        try {
            target.load(path);
        } catch (e) {
            if (e.message.startsWith("GlobalObject.load: cannot open file")) {
                for (var i = 0; i < subPathList.length; i ++) {
                    try {
                        target.load(subPathList[i] + path);
                        return target;
                    } catch (e) {
                    }
                }
            }
            throw e;
        }
        return target;
    },
    Arguments : ["summary"],
    Quit : function(code) {
        throw Error(code);
    },
};

function CollectGarbage() {
    gc();
}
