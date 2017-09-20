function Component() {}

Component.prototype.createOperations = function() {
    try {
        // call the base create operations function
        component.createOperations();
        component.addOperation(
            "CreateShortcut",
            "@TargetDir@/qaccelerator.exe",
            "@StartMenuDir@/QAccelerator.lnk");
        component.addOperation(
            "CreateShortcut",
            "@TargetDir@/qaccelerator.exe",
            "@HomeDir@/Desktop/QAccelerator.lnk");
    } catch (e) {
        console.log(e);
    }
}
