/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
package com.cavisson.jenkins;

import java.util.ResourceBundle;

/**
 *
 * @author preety.yadav
 */
public enum LocalMessages {
    
PROJECTACTION_DISPLAYNAME("NetStormProjectAction.DisplayName"),
BUILDACTION_DISPLAYNAME("NetStormBuildAction.DisplayName"),
ND_BUILDACTION_DISPLAYNAME("NetDiagnosticsBuildAction.DisplayName"),
NSND_BUILDACTION_DISPLAYNAME("NSNDIntegrationBuildAction.DisplayName"),
PUBLISHER_DISPLAYNAME("NetStormResultsPublisher.DisplayName"),
ND_PUBLISHER_DISPLAYNAME("NetDiagnosticsResultsPublisher.DisplayName"),
NSND_PUBLISHER_DISPLAYNAME("NSNDIntegrationResultsPublisher.DisplayName"),
REPORT_DISPLAYNAME("NetStormReport.DisplayName"),
ND_REPORT_DISPLAYNAME("NetDiagnosticsReport.DisplayName");
private final static ResourceBundle MESSAGES = ResourceBundle.getBundle("jenkins.pipeline.Messages");
private final String msgRef;
private LocalMessages(final String msgReference) {
msgRef = msgReference;
}
@Override
public String toString() {
return MESSAGES.getString(msgRef);
}
    
}
