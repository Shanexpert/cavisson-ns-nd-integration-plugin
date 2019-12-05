/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
package com.cavisson.jenkins;

import hudson.model.Run;
import hudson.model.Action;
import hudson.util.StreamTaskListener;
import java.io.IOException;
import java.lang.ref.WeakReference;
import java.util.logging.Level;
import java.util.logging.Logger;
import org.kohsuke.stapler.StaplerProxy;

/**
 *
 * @author preety.yadav
 */
public class NetStormBuildAction implements Action, StaplerProxy {
    
  private final Run<?, ?> build;
  private final NetStormReport report;
  private transient WeakReference<BuildActionResultsDisplay> buildActionResultsDisplay;
  private boolean isNDE = false;
  private boolean isIntegrated = false;

  private transient static final Logger logger = Logger.getLogger(NetStormBuildAction.class.getName());

  public NetStormBuildAction(Run<?, ?> build, NetStormReport report) {
    this.build = build;
    this.report = report;
  }
  
  public NetStormBuildAction(Run<?, ?> build, NetStormReport report, boolean isNDE) {
	    this.build = build;
	    this.report = report;
	    this.isNDE = isNDE;
  }
  
   public NetStormBuildAction(Run<?, ?> build, NetStormReport report, boolean isNDE, boolean isIntegrated) {
	    this.build = build;
	    this.report = report;
	    this.isNDE = isNDE;
            this.isIntegrated = isIntegrated;
  }
  

    @Override
    public String getIconFileName() {
          return "graph.gif";
    }

    @Override
    public String getDisplayName() {
        if(isNDE)
	 return LocalMessages.ND_BUILDACTION_DISPLAYNAME.toString();
        else if (isIntegrated)
          return LocalMessages.NSND_BUILDACTION_DISPLAYNAME.toString();
	else
         return LocalMessages.BUILDACTION_DISPLAYNAME.toString();
    }

    @Override
    public String getUrlName() {
        
        if(isNDE)
	 return "netdiagonstic-dashboard";
        else
          return "netstorm-dashboard";
    }

    @Override
    public BuildActionResultsDisplay getTarget() {
      return getBuildActionResultsDisplay();
  }
    
  
    public Run<?, ?> getBuild() {
    return build;
  }

  public NetStormReport getNetStormReport() {
    return report;
  }

  public BuildActionResultsDisplay getBuildActionResultsDisplay() {
	
    BuildActionResultsDisplay buildDisplay = null;
    WeakReference<BuildActionResultsDisplay> wr = this.buildActionResultsDisplay;
    
    if (wr != null) {
      buildDisplay = wr.get();
      if (buildDisplay != null)
        return buildDisplay;
    }

    try {
      buildDisplay = new BuildActionResultsDisplay(this, StreamTaskListener.fromStdout(), isNDE);
    } catch (IOException e) {
      logger.log(Level.SEVERE, "Error creating new BuildActionResultsDisplay()", e);
    }
    this.buildActionResultsDisplay = new WeakReference<BuildActionResultsDisplay>(buildDisplay);
    return buildDisplay;
  }
  
  
  public void setBuildActionResultsDisplay(WeakReference<BuildActionResultsDisplay> buildActionResultsDisplay) {
    this.buildActionResultsDisplay = buildActionResultsDisplay;
  }

       
}
