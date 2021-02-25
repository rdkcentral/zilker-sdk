# Automation content for unit tests

The files in this directory are used by the automation service unit tests.  They start as .yaml files
which are converted into JSON via a command-line tool called 'yaml2json'.  Since that tool isnt available
everywhere, we check in the resulting .js files here.
