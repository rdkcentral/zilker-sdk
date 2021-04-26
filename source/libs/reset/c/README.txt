Library providing functions for resetting and rebooting

One could argue that this should reside in util lib, however these
functions need to coordinate with other services and therefore cannot
be part of the 'base' library.

Additionally, this has a dependency on propsService API

