package com.cavisson.jenkins;

import java.io.IOException;
import java.lang.ref.WeakReference;
import java.util.logging.Level;
import java.util.logging.Logger;

import org.kohsuke.stapler.StaplerProxy;

import hudson.model.Action;
import hudson.model.Run;
import hudson.util.StreamTaskListener;

public class NetStormStopThread implements Action, StaplerProxy {

	public final Run<?, ?> build;
	private transient WeakReference<BuildActionStopThread> buildActionResultsDisplay;
	private transient final Logger logger = Logger.getLogger(NetStormStopThread.class.getName());

    public NetStormStopThread(Run<?, ?> run) {
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
        return "Stop Test Thread";
    }

    @Override
    public String getUrlName() {
        return "stop-test";
    }
    
    @Override
    public BuildActionStopThread getTarget() {
      return getBuildActionResultsDisplay();
  }
    
  
    public Run<?, ?> getBuild() {
        return build;
      }

//    public int getNetStormTest() {
//    	return NetStormStopAction.testrun;
//    }
    
    public BuildActionStopThread getBuildActionResultsDisplay() {
    	
    	BuildActionStopThread buildDisplay = null;
        WeakReference<BuildActionStopThread> wr = this.buildActionResultsDisplay;
        
        if (wr != null) {
          buildDisplay = wr.get();
          if (buildDisplay != null)
            return buildDisplay;
        }

        try {
          buildDisplay = new BuildActionStopThread(this, StreamTaskListener.fromStdout());
        } catch (IOException e) {
          logger.log(Level.SEVERE, "Error creating new BuildActionStopTest()", e);
        }
        this.buildActionResultsDisplay = new WeakReference<BuildActionStopThread>(buildDisplay);
        return buildDisplay;
      }
      
      
      public void setBuildActionResultsDisplay(WeakReference<BuildActionStopThread> buildActionStopTest) {
        this.buildActionResultsDisplay = buildActionStopTest;
      }

}
