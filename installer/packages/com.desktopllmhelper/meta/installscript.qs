function Component()
{
    installer.finishButtonClicked.connect(this, Component.prototype.launchApplication);
}

Component.prototype.launchApplication = function()
{
    try {
        if (installer.isInstaller() && installer.status === QInstaller.Success) {
            var targetDir = installer.value("TargetDir");
            var exePath = installer.toNativeSeparators(targetDir + "/DesktopLLMHelper.exe");
            installer.executeDetached(exePath, [], installer.toNativeSeparators(targetDir));
        }
    } catch(e) {
        console.log("Failed to launch application: " + e);
    }
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
