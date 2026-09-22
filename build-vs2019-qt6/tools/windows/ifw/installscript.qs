function Component() {}

Component.prototype.createOperations = function() {
    component.createOperations();
    if (systemInfo.productType === "windows" && installer.value("CreateShortcuts", "true") !== "false") {
        component.addOperation("CreateShortcut", "@TargetDir@/Boarder.exe",
            "@StartMenuDir@/B-Line.lnk", "workingDirectory=@TargetDir@", "description=Launch B-Line");
        component.addOperation("CreateShortcut", "@TargetDir@/Boarder.exe",
            "@DesktopDir@/B-Line.lnk", "workingDirectory=@TargetDir@", "description=Launch B-Line");
    }
};
