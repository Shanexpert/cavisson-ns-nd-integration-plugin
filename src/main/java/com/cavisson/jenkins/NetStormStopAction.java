package com.cavisson.jenkins;

import hudson.model.Action;
import hudson.model.Run;
import hudson.model.Result;
import hudson.util.StreamTaskListener;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.lang.ref.WeakReference;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.logging.Level;
import java.util.logging.Logger;
import org.kohsuke.stapler.bind.JavaScriptMethod;
import org.kohsuke.stapler.StaplerProxy;

public class NetStormStopAction implements Action, StaplerProxy {

	public final Run<?, ?> build;
	private transient WeakReference<BuildActionStopTest> buildActionResultsDisplay;
	private transient final Logger logger = Logger.getLogger(NetStormStopAction.class.getName());

    public NetStormStopAction(Run<?, ?> run) {
    	this.build = run;
    }
    
//    public int getTestrun() {
//		return testrun;
//	}
//    
//    public void setTestrun(int testrun) {
//		this.testrun = testrun;
//	}
//    
//    public String getUsername() {
//		return username;
//	}
//
//	public void setUsername(String username) {
//		this.username = username;
//	}

	@Override
    public String getIconFileName() {
        return "graph.gif";
    }

    @Override
    public String getDisplayName() {
        return "Stop Job";
    }

    @Override
    public String getUrlName() {
        return "stop-job";
    }
    
    @Override
    public BuildActionStopTest getTarget() {
      return getBuildActionResultsDisplay();
  }
    
  
    public Run<?, ?> getBuild() {
        return build;
      }

//    public int getNetStormTest() {
//    	return NetStormStopAction.testrun;
//    }
    
    public BuildActionStopTest getBuildActionResultsDisplay() {
    	
    	BuildActionStopTest buildDisplay = null;
        WeakReference<BuildActionStopTest> wr = this.buildActionResultsDisplay;
        
        if (wr != null) {
          buildDisplay = wr.get();
          if (buildDisplay != null)
            return buildDisplay;
        }

        try {
          buildDisplay = new BuildActionStopTest(this, StreamTaskListener.fromStdout());
        } catch (IOException e) {
          logger.log(Level.SEVERE, "Error creating new BuildActionStopTest()", e);
        }
        this.buildActionResultsDisplay = new WeakReference<BuildActionStopTest>(buildDisplay);
        return buildDisplay;
      }
      
      
      public void setBuildActionResultsDisplay(WeakReference<BuildActionStopTest> buildActionStopTest) {
        this.buildActionResultsDisplay = buildActionStopTest;
      }

}
