/*
 * Copyright 2021 Comcast Cable Communications Management, LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package com.comcast.xh.zith.client;

import com.icontrol.api.properties.CommonProperties;
import com.icontrol.api.watchdog.ShutdownOptions;
import com.icontrol.api.watchdog.WatchdogServiceClient;
import com.icontrol.api.watchdog.WatchdogService_IPC;
import com.icontrol.ipc.impl.IPCException;
import com.comcast.xh.zith.template.TemplateEngine;
import freemarker.template.TemplateException;
import org.apache.commons.io.FileUtils;
import org.apache.commons.io.IOUtils;
import org.apache.commons.lang.StringUtils;
import org.apache.commons.lang.SystemUtils;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.*;
import java.lang.reflect.Field;
import java.net.ServerSocket;
import java.util.HashMap;
import java.util.Map;
import java.util.Random;
import java.util.concurrent.TimeUnit;
import java.util.TimeZone;

public class NativeZilkerClient implements ZilkerClient
{
    // TODO FIXME 15 seconds is too long, we want things to shutdown quickly, but due to IPC shutdown timing issues
    //      right now some shutdowns are taking longer than they should
    public static final int SHUTDOWN_TIMEOUT_SECS = 15;
    public static String upgEnv = null;
    private static Logger log = LoggerFactory.getLogger(NativeZilkerClient.class);



    private boolean singleProcess = true;
    private File zilkerTop;
    private File configDir = null;
    private boolean cleanShutdown = true;
    private Process process;
    private String cpeId;
    private ClientConfigCustomizer configCustomizer = null;
    private Map<String,String> genericProps = null;
    private boolean includePackagedWhitelist = false;
    private boolean startActivated = false;

    private Thread outPump;
    private Thread errPump;

    public NativeZilkerClient()
    {
        this(null);
    }

    public NativeZilkerClient(String configDir)
    {
        genericProps = getDefaultGenericProps();
        if (configDir != null)
        {
            this.configDir = new File(configDir);
        }

        // Go ahead and generate our cpeId
        Random r = new Random();
        long l = r.nextLong();
        // Make sure its not more than 48 bits
        l = l >>> 16;
        cpeId = String.format("%012x", l);
    }

    public String getCpeId()
    {
        return cpeId;
    }

    private Map<String,String> getDefaultGenericProps()
    {
        Map<String,String> defaultProps = new HashMap<String,String>();
        defaultProps.put("cpe.dev.geocoder.url.override", "https://127.0.0.1:8443/maps/api/geocode/json?address");
        defaultProps.put(CommonProperties.CPE_GEOCODING_ENABLED_BOOL, "false");
        defaultProps.put("cpe.dnd.default.flag", "false");
        defaultProps.put("cpe.codebig.useproduction", "false");
        defaultProps.put("camera.discovery.ipPrefix","localhost");
        defaultProps.put("cpe.sslCert.validateForHttpsToServer","none");
        defaultProps.put("cpe.sslCert.validateForHttpsToDevice","none");
        defaultProps.put("cpe.sslCert.validateForXmpp","none");
        defaultProps.put("cpe.camera.pingIntervalSeconds","1");
        defaultProps.put("cpe.tests.fastTimers", "true");
        return defaultProps;
    }

    public void setGenericPropConfig(String key, String value)
    {
        genericProps.put(key, value);
    }

    public void setClientConfigCustomizer(ClientConfigCustomizer configCustomizer)
    {
        this.configCustomizer = configCustomizer;
    }

    public void setIncludePackagedWhitelist(boolean includePackagedWhitelist)
    {
        this.includePackagedWhitelist = includePackagedWhitelist;
    }

    public void setStartActivated(boolean startActivated)
    {
        this.startActivated = startActivated;
        if (startActivated == true)
        {
            // Just put what's required to say we are activated and keep from doing much extra work
            genericProps.put("deviceDescriptorList", "https://localhost:8443/bundle/lists/WhiteList-Converge.xml");
            genericProps.put("persist.cpe.setupwizard.state","3");
            genericProps.put("domicileId","101");
        }
    }

    private boolean isDebug()
    {
        return Boolean.parseBoolean(System.getProperty("zith.client.debug"));
    }

    private void resetConfig()
    {
        try
        {
            if (configDir.exists())
            {
                FileUtils.deleteDirectory(configDir);
            }
            configDir.mkdir();
            populateConfig();
        } catch (IOException e)
        {
            throw new RuntimeException("Could not prepare config directory " + configDir.getAbsolutePath());
        }
    }

    protected static void copyResourceToDir(String resourceName, File dest) throws IOException
    {
        try(
                InputStream is = NativeZilkerClient.class.getResourceAsStream(resourceName);
                FileOutputStream fos = new FileOutputStream(dest);
        )
        {
            IOUtils.copy(is, fos);
            fos.flush();
        }
    }

    private void populateConfig() throws IOException
    {
        File etcDir = new File(configDir, "etc");
        etcDir.mkdir();
        File macAddressFile = new File(etcDir, "macAddress");
        try (FileOutputStream fos = new FileOutputStream(macAddressFile))
        {
            fos.write(cpeId.getBytes("UTF-8"));
            fos.flush();
        }

        // Place communication.conf into etc directory
        File communicationConf = new File(etcDir, "communication.conf");
        if (startActivated == false)
        {
            copyResourceToDir("/client/communication.conf", communicationConf);
        }
        else
        {
            copyResourceToDir("/client/activatedCommunication.conf", communicationConf);
        }

        upgEnv = getConfigDir().getAbsolutePath().concat("/tmp/upg/");
        setGenericPropConfig("IC_UPG_CACHE", upgEnv);
        // Generic props
        try
        {
            File genericPropsFile = new File(etcDir, "genericProps.xml");
            Map<String,Object> model = new HashMap<>();
            model.put("genericProps", genericProps);
            TemplateEngine.getInstance().writeClasspathTemplate("client/genericProps.xml.ftl", model, genericPropsFile);
        } catch (TemplateException e)
        {
            throw new IOException(e);
        }

        File brandingFile = new File(etcDir, "branding");
        try (FileOutputStream fos = new FileOutputStream(brandingFile))
        {
            fos.write("icontrol".getBytes("UTF-8"));
            fos.flush();
        }

        if (includePackagedWhitelist == true || startActivated == true)
        {
            File whiteListFile = new File(etcDir, "WhiteList.xml");
            copyResourceToDir("/client/WhiteList.xml", whiteListFile);
        }

        if (configCustomizer != null)
        {
            configCustomizer.customizeConfig(configDir);
        }

        File cronDir = new File(etcDir, "cron");
        cronDir.mkdir();
        File crontabDir = new File(cronDir, "crontabs");
        crontabDir.mkdir();
    }

    protected File createConfigDir() throws IOException
    {
        // Create config directory and values
        File configDir = File.createTempFile("zilkerConfig", "");
        configDir.delete();
        configDir.mkdir();
        return configDir;
    }

    private void setupConfig() throws IOException
    {
        configDir = createConfigDir();
        // Make sure we don't need to a reference to the client to hang around
        final File paramConfigDir = configDir;
        Runtime.getRuntime().addShutdownHook(new Thread() {
            @Override
            public void run()
            {
                try {
                    FileUtils.deleteDirectory(paramConfigDir);
                } catch (IOException e)
                {
                    // oh well
                }
            }
        });

        populateConfig();
    }

    public static File determineZilkerTop()
    {
        String zilkerTop = System.getenv("ZILKER_SDK_TOP");
        if (zilkerTop == null || zilkerTop.isEmpty())
        {
            // try to figure it out based on relative from where we are
            File cwd = new File(System.getProperty("user.dir"));
            // Assume launched from zith directory
            zilkerTop = cwd.getParentFile().getParentFile().getAbsolutePath();
            // TODO attempt to validate
        }

        if (zilkerTop != null && !zilkerTop.isEmpty())
        {
            return new File(zilkerTop);
        }
        else
        {
            throw new RuntimeException("Failed to determine ZILKER_SDK_TOP");
        }
    }

    protected boolean isConflictingClientRunning()
    {
        boolean isRunning = true;
        // This is pretty cheesy and non-blackbox, but try to bind to a well known IPC port, and if we can't then
        // assume zilker client is running
        ServerSocket ss = null;
        try
        {
            ss = new ServerSocket(WatchdogService_IPC.WATCHDOGSERVICE_IPC_PORT_NUM);
            isRunning = false;
        } catch (IOException e)
        {
            log.error("Failed to bind to watchdog port, ZilkerClient might be running already", e);

        } finally
        {
            try
            {
                if (ss != null)
                {
                    ss.close();
                }
            } catch (IOException e)
            {
                // Oh well
            }
        }

        return isRunning;
    }

    protected boolean isProcessStarted()
    {
        return process != null;
    }

    public static File findZilkerBinary(File zilkerTop)
    {
        File retval;
        // TODO For running with server builds we just want to enable building zilker for single process,
        // but it wouldn't necessarily be at this path....
        String zilkerBin = System.getProperty("zith.zilker.bin.path");
        if (StringUtils.isEmpty(zilkerBin))
        {
            zilkerBin = zilkerTop.getAbsolutePath() + "/cmake-build-debug/zilker";
            retval = new File(zilkerBin);
            if (!retval.exists())
            {
                // Fall back to command line build path
                zilkerBin = zilkerTop.getAbsolutePath() + "/build/angelsenvy/zilker";
                retval = new File(zilkerBin);
                if (!retval.exists())
                {
                    throw new RuntimeException("Failed to find zilker binary to use");
                }
            }
        }
        else
        {
            retval = new File(zilkerBin);
        }

        return retval;
    }

    protected void startProcess() throws Exception
    {
        File zilkerExec = findZilkerBinary(getZilkerTop());
        if (!zilkerExec.exists())
        {
            throw new RuntimeException(
                    "Zilker client must be built for CLion, no zilker executable found at " + zilkerExec.getAbsolutePath());
        }
        String mirrorDir = System.getProperty("zith.mirror.dir.path");
        if (StringUtils.isEmpty(mirrorDir))
        {
            mirrorDir = getZilkerTop().getAbsolutePath() + "/build/angelsenvy/mirror";
        }

        ProcessBuilder pb =
                new ProcessBuilder(zilkerExec.getAbsolutePath(), "-c", configDir.getAbsolutePath(), "-h", mirrorDir);
        Map<String, String> env = pb.environment();

        env.put("UPG_CACHE", upgEnv);

        if (SystemUtils.IS_OS_MAC)
        {
            // rpath no workie on Mac, so need to force the DYLD_LIBRARY_PATH
            env.put("DYLD_LIBRARY_PATH", mirrorDir+"/lib");
        }
        process = pb.start();

        /* Write stdout,err through our test process so JUnit can capture and save them with test reports */
        InputStream is = getProcessInputStream();
        if (is != null)
        {
            outPump = createIOPump(is, System.out);
        }
        InputStream es = getProcessErrorStream();
        if (es != null)
        {
            errPump = createIOPump(es, System.err);
        }

        TimeZone tzone = TimeZone.getTimeZone("America/Chicago");
        tzone.setDefault(tzone);
    }

    protected InputStream getProcessInputStream()
    {
        return process.getInputStream();
    }

    protected InputStream getProcessErrorStream()
    {
        return process.getErrorStream();
    }

    public void start()
    {
        if (isDebug())
        {
            return;
        }

        try
        {
            if (configDir == null)
            {
                setupConfig();
            }

            if (singleProcess)
            {
                if (isConflictingClientRunning())
                {
                    throw new IllegalStateException("Zilker Client is already running");
                }

                startProcess();
            }
            else
            {
                // TODO How would we detect all the child process exit codes if we weren't running the single process version?
                throw new RuntimeException("Only single process mode is currently supported");
            }
        } catch (Exception e)
        {
            throw new RuntimeException("Failed to start", e);
        }
    }

    protected static Thread createIOPump(InputStream from, OutputStream to)
    {
        if (from == null)
        {
            throw new IllegalArgumentException("from cannot be null");
        }

        if (to == null)
        {
            throw new IllegalArgumentException("to cannot be null");
        }

        final InputStream source = new BufferedInputStream(from);
        final OutputStream sink = new BufferedOutputStream(to);

        Thread pump = new Thread(() -> {
            int data;
            try
            {
                while ((data = source.read()) != -1)
                {
                    sink.write(data);
                    if (data == '\n')
                    {
                        sink.flush();
                    }
                }
            } catch (IOException e)
            {
                log.error("Unable to read from stream: " + e);
            }
        });

        pump.start();
        return pump;
    }

    public int restart()
    {
        int exitCode = stop();
        if (exitCode == 0)
        {
            start();
        }

        return exitCode;
    }

    private File getGDBPath()
    {
        // TODO this could be done better(capture output of bash -c env gdb)
        return new File("/usr/bin/gdb");
    }

    public void doBacktrace()
    {
        File gdbPath = getGDBPath();
        if (gdbPath != null && gdbPath.exists() && process.getClass().getName().equals("java.lang.UNIXProcess"))
        {
            try
            {
                Field f = process.getClass().getDeclaredField("pid");
                f.setAccessible(true);
                long pid = f.getLong(process);
                f.setAccessible(false);
                // Setup the list of commands for gdb to run
                File gdbCommands = File.createTempFile("gdb", "out");
                gdbCommands.deleteOnExit();
                try (FileWriter fw = new FileWriter(gdbCommands))
                {
                    fw.write("set confirm off\n");
                    fw.write("set pagination off\n");
                    fw.write("info threads\n");
                    fw.write("thread apply all bt\n");
                    fw.write("quit\n");
                }
                // Have to run via sudo because of security stuff with ptrace...assumes user has setup user to not require password for sudo
                ProcessBuilder pb =
                        new ProcessBuilder("sudo", "gdb", "-p", Long.toString(pid), "-x", gdbCommands.getAbsolutePath());

                pb.inheritIO();
                Process p = pb.start();
                try
                {
                    p.waitFor(5, TimeUnit.SECONDS);
                } catch (Exception e)
                {
                    log.warn("Failed to get backtrace, gdb timed out", e);
                    p.destroy();
                }
            }
            catch (Exception e1)
            {
                log.warn("Failed to get process ID for backtrace", e1);
            }
        }
        else
        {
            log.warn("Could not retrieve backtrace for zilker client");
        }
    }

    protected int waitForProcessExit() throws Exception
    {
        process.waitFor(SHUTDOWN_TIMEOUT_SECS, TimeUnit.SECONDS);
        int exitVal = process.exitValue();
        process = null;
        return exitVal;
    }

    protected void killProcess()
    {
        process.destroy();
        process = null;
    }

    public int stop()
    {
        int exitCode = 0;
        if (isProcessStarted())
        {
            if (cleanShutdown)
            {
                ShutdownOptions options = new ShutdownOptions();
                options.setExit(true);
                try
                {
                    try
                    {
                        WatchdogServiceClient.shutdownAllServices(options);
                    } catch (IPCException e)
                    {
                        // The socket may receive EOF with no response when watchdog service self destructs.
                        // Ignore this and instead wait for the process to exit with the correct OK (0) code.
                        log.warn("zilker shutdown request received an IPC exception." +
                                 "The process will likely exit normally.", e);
                    }

                    exitCode = waitForProcessExit();
                } catch (Exception e)
                {
                    log.error("Failed to shutdown zilker client", e);
                    doBacktrace();
                    // Kill it
                    killProcess();
                    exitCode = -1;
                }
            }
            else
            {
                // Just kill it and say all is well
                killProcess();
                exitCode = 0;
            }

            log.info("Process exited with status {}", exitCode);

            try
            {
                if (errPump != null)
                {
                    errPump.join();
                }
                if (outPump != null)
                {
                    outPump.join();
                }
            }
            catch (InterruptedException e)
            {
                log.warn("Interrupted while waiting for IO threads to join");
            }

            errPump = null;
            outPump = null;
        }

        return exitCode;
    }

    public boolean isSingleProcess()
    {
        return singleProcess;
    }

    public void setSingleProcess(boolean singleProcess)
    {
        this.singleProcess = singleProcess;
    }

    private File getZilkerTop()
    {
        if (zilkerTop == null)
        {
            zilkerTop = determineZilkerTop();
        }
        return zilkerTop;
    }

    public File getConfigDir()
    {
        return configDir;
    }

    public void setConfigDir(File configDir)
    {
        this.configDir = configDir;
    }

    public boolean isCleanShutdown()
    {
        return cleanShutdown;
    }

    public void setCleanShutdown(boolean cleanShutdown)
    {
        this.cleanShutdown = cleanShutdown;
    }

    public static void main(String[] args)
    {
        String configDir = null;
        // This is just for Clion to call to setup a config directory for us to use
        if (args.length != 1)
        {
            configDir = System.getProperty("zith.config.dir");
            if (configDir == null || configDir.isEmpty())
            {
                throw new IllegalArgumentException("configDir not passed");
            }
        }
        else
            {
            configDir = args[0];
        }
        NativeZilkerClient client = new NativeZilkerClient(configDir);
        client.resetConfig();
    }
}
