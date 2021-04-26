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

package com.comcast.xh.zith;

import ch.qos.logback.classic.Level;
import ch.qos.logback.classic.LoggerContext;
import ch.qos.logback.classic.joran.JoranConfigurator;
import com.comcast.xh.zith.mockzigbeecore.MockZigbeeCore;
import org.jline.reader.LineReader;
import org.jline.terminal.Terminal;
import org.jline.terminal.impl.DumbTerminal;
import org.jline.utils.AttributedStringBuilder;
import org.jline.utils.AttributedStyle;
import org.jline.utils.AttributedString;
import org.jline.utils.Status;
import org.slf4j.LoggerFactory;
import org.slf4j.bridge.SLF4JBridgeHandler;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.autoconfigure.context.PropertyPlaceholderAutoConfiguration;
import org.springframework.context.ApplicationContext;
import org.springframework.context.annotation.*;
import org.springframework.shell.ExitRequest;
import org.springframework.shell.InputProvider;
import org.springframework.shell.Shell;
import org.springframework.shell.SpringShellAutoConfiguration;
import org.springframework.shell.jcommander.JCommanderParameterResolverAutoConfiguration;
import org.springframework.shell.jline.InteractiveShellApplicationRunner;
import org.springframework.shell.jline.JLineShellAutoConfiguration;
import org.springframework.shell.jline.PromptProvider;
import org.springframework.shell.standard.ShellComponent;
import org.springframework.shell.standard.ShellMethod;
import org.springframework.shell.standard.StandardAPIAutoConfiguration;
import org.springframework.shell.standard.commands.Quit;
import org.springframework.shell.standard.commands.StandardCommandsAutoConfiguration;
import org.springframework.stereotype.Component;

import javax.annotation.PostConstruct;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.List;

@Configuration
@ComponentScan(basePackages = "com.comcast.xh.zith")
@Import({
        // Core runtime
        SpringShellAutoConfiguration.class, // required
        JLineShellAutoConfiguration.class, // required
        // Various Resolvers
        JCommanderParameterResolverAutoConfiguration.class, // required
        //LegacyAdapterAutoConfiguration.class,
        StandardAPIAutoConfiguration.class, // required
        // Built-In Commands
        StandardCommandsAutoConfiguration.class, // required
        // Allows ${} support
        PropertyPlaceholderAutoConfiguration.class,
        // Our Custom Commands

})
@ShellComponent
public class ZithShell
{
    public static void main(String[] args) throws IOException
    {
        // Explicitly force for dumb terminal if no tty
        if (System.console() == null)
        {
            System.setProperty("org.jline.terminal.dumb", "true");
            System.setProperty("org.jline.terminal.type", DumbTerminal.TYPE_DUMB);
        }
        configureLogging();
        ApplicationContext context = new AnnotationConfigApplicationContext(ZithShell.class);
        Shell shell = context.getBean(Shell.class);
        shell.run(context.getBean(InputProvider.class));
    }

    @Bean
    @Autowired
    public InputProvider inputProvider(LineReader lineReader, PromptProvider promptProvider) {
        return new InteractiveShellApplicationRunner.JLineInputProvider(lineReader, promptProvider);
    }

    @Bean
    public StatusLine statusLine()
    {
        return new StatusLine();
    }

    private static class StatusLine
    {
        private static AttributedStyle STATUS_STYLE = AttributedStyle.INVERSE;
        private static AttributedStyle RUNNING_STYLE = AttributedStyle.INVERSE.bold().background(AttributedStyle.GREEN);
        private static AttributedStyle STOPPED_STYLE = AttributedStyle.INVERSE.bold().background(AttributedStyle.RED);

        @Autowired
        private MockZigbeeCore mockZigbeeCore;

        @Autowired
        private Terminal terminal;

        private Status status;

        @PostConstruct
        private void init() throws IOException
        {
            status = Status.getStatus(terminal, true);
            mockZigbeeCore.setStatusListener((boolean running)->{
                updateStatus(false, false, running);
            });
            updateStatus(false, false, mockZigbeeCore.isRunning());
        }

        public void updateStatus(boolean serverRunning, boolean clientConnected, boolean zigbeeRunning)
        {
            List<AttributedString> statusLines = new ArrayList<>();
            AttributedStringBuilder sb = new AttributedStringBuilder();
            sb.styled(STATUS_STYLE,"Server: ");
            if (serverRunning)
            {
                sb.styled(RUNNING_STYLE, "RUNNING");
                sb.styled(STATUS_STYLE, " | Client: ");
                if (clientConnected)
                {
                    sb.styled(RUNNING_STYLE, "CONNECTED");
                }
                else
                {
                    sb.styled(STOPPED_STYLE, "DISCONNECTED");
                }
            }
            else
            {
                sb.styled(STOPPED_STYLE, "STOPPED");
            }
            sb.styled(STATUS_STYLE, " | Zigbee: ");
            if (zigbeeRunning)
            {
                sb.styled(RUNNING_STYLE, "RUNNING");
            }
            else
            {
                sb.styled(STOPPED_STYLE, "STOPPED");
            }
            int termWidth = terminal.getWidth();
            for(int i = sb.toString().length(); i < termWidth; ++i)
            {
                sb.styled(STATUS_STYLE," ");
            }
            statusLines.add(sb.toAttributedString());
            status.update(statusLines);
        }
    }

    @Component
    private static class ZithPromptProvider implements PromptProvider
    {
        private static AttributedStyle PROMPT_STYLE = AttributedStyle.DEFAULT.foreground(AttributedStyle.YELLOW);

        @Autowired
        MockZigbeeCore mockZigbeeCore;

        public AttributedString getPrompt()
        {
            AttributedStringBuilder sb = new AttributedStringBuilder();
            sb.styled(PROMPT_STYLE, "zith");
            sb.styled(PROMPT_STYLE, ">");
            return sb.toAttributedString();
        }
    }

    @Bean
    PromptProvider promptProvider()
    {
        return new ZithPromptProvider();
    }

    @Bean
    MockZigbeeCore mockZigbeeCore()
    {
        return MockZigbeeCore.getInstance();
    }

    @ShellMethod("Start MockZigbeeCore")
    public void startAll()
    {
        mockZigbeeCore().start();
    }

    @ShellMethod("Stop MockZigbeeCore")
    public void stopAll()
    {
        if (mockZigbeeCore().isRunning())
        {
            mockZigbeeCore().stop();
        }
//        MockCamera.destroyCameras();
    }

    @ShellMethod(value="Enable debug logging", key="debug-enable")
    public void enableDebug()
    {
        // TODO this doesn't seem to work
        LoggerContext loggerContext = (LoggerContext) LoggerFactory.getILoggerFactory();
        loggerContext.getLogger(MockZigbeeCore.class).setLevel(Level.DEBUG);
    }

    @ShellMethod(value="Disable debug logging", key="debug-disable")
    public void disableDebug()
    {
        // TODO this doesn't seem to work
        LoggerContext loggerContext = (LoggerContext) LoggerFactory.getILoggerFactory();
        loggerContext.getLogger(MockZigbeeCore.class).setLevel(Level.ERROR);
    }

    private static void configureLogging()
    {
        java.util.logging.Logger.getLogger("").setLevel(java.util.logging.Level.FINEST);
        SLF4JBridgeHandler.removeHandlersForRootLogger();
        SLF4JBridgeHandler.install();
        try
        {
            LoggerContext loggerContext = (LoggerContext) LoggerFactory.getILoggerFactory();
            loggerContext.reset();
            JoranConfigurator configurator = new JoranConfigurator();
            InputStream configStream = ZithShell.class.getResourceAsStream("/logback-console.xml");
            configurator.setContext(loggerContext);
            configurator.doConfigure(configStream); // loads logback file
            configStream.close();
        } catch (Exception e)
        {
            e.printStackTrace();
        }
    }

    @ShellComponent
    private class QuitCommand implements Quit.Command
    {
        @ShellMethod(value = "Exit the shell.", key = {"quit", "exit"})
        public void quit()
        {
            // Have to do this to prevent the shell from hanging on shutdown
            stopAll();
            throw new ExitRequest();
        }
    }
}
