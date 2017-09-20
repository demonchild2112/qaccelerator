var AUTO_INSTALL_ENV_VAR = 'AUTO_INSTALL_QACCELERATOR'

function Controller() {
  installer.setDefaultPageVisible(QInstaller.ComponentSelection, false);
}

function autoInstall() {
  return installer.environmentVariable(AUTO_INSTALL_ENV_VAR) == 'true'
}

function clickNext() {
  if (autoInstall()) {
    gui.clickButton(buttons.NextButton)
  }
}

Controller.prototype.IntroductionPageCallback = function() {
  var widget = gui.pageById(QInstaller.Introduction);
  widget.title = "QAccelerator Setup";
  arch = systemInfo.currentCpuArchitecture
  widget.MessageLabel.setText("Architecture: " + arch);
  clickNext();
};

Controller.prototype.TargetDirectoryPageCallback = clickNext;

Controller.prototype.ComponentSelectionPageCallback = clickNext;

Controller.prototype.LicenseAgreementPageCallback = function() {
  if (autoInstall()) {
    gui.currentPageWidget().AcceptLicenseRadioButton.setChecked(true);
    clickNext();
  }
};

Controller.prototype.StartMenuDirectoryPageCallback = clickNext;

Controller.prototype.ReadyForInstallationPageCallback = clickNext;

Controller.prototype.FinishedPageCallback = function() {
  if (autoInstall()) {
    gui.clickButton(buttons.FinishButton);
  }
};
