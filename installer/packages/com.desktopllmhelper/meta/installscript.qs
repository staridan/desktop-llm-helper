function Component()
{
    // empty constructor
}

Component.prototype.createOperations = function()
{
    // perform the default file-install operations
    component.createOperations();

    // on Windows, add a shortcut to the Startup folder for autorun
    if (systemInfo.productType === "windows") {
        component.addOperation(
            "CreateShortcut",
            "@TargetDir@/DesktopLLMHelper.exe",
            "@UserStartMenuProgramsPath@/Startup/DesktopLLMHelper.lnk",
            "workingDirectory=@TargetDir@",
            "iconPath=@TargetDir@/DesktopLLMHelper.exe"
        );
    }
}