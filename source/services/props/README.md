# Props Service Overview

The *props service* is a storage facility for key/value pairs of information, 
which are represented in `property` objects.  Each `property` has the following 
attributes:

~~~
typedef struct _property {
    Pojo       base;    // Base object; use pojo.h interface 
    char       *key;                                         
    char       *value;                                       
    propSource source;                                       
} property;
~~~

The `source` is a hierarchical ranking that not only documents where
the property came from, but also if it's allowed to be overwritten by
a different source:
~~~
// describes priority ranking when saving properties (larger is higher priority)
typedef enum {
     PROPERTY_SRC_DEFAULT  = 0,  
     PROPERTY_SRC_SERVER   = 1,  
     PROPERTY_SRC_XCONF    = 2,  
     PROPERTY_SRC_DEVICE   = 3   
} propSource;
~~~

For example, a property with a `source` of `PROPERTY_SRC_DEVICE` **can not**
be overwritten by the source `PROPERTY_SRC_SERVER`.

## Startup

After loading the configuration file, the service will iterate through all 
property definitions contained in the `/vendor/etc/defaults/globalSettings.properties`
file.  This allows us to define per-brand default properties without hard-coding them.
Each item in the file will **only generate** a new `property` object if missing from the
configuration.

## Property Definition

Each property should have a property definition in a json file.  The reason for defining
property definitions is to allow for validation of the property values, and does not
imply any sort of defaults for property values. Each service or utility
that utilize properties should define one or more property definition files in a directory
called `propDefs` at the same level as the service or utility's `src` directory.  A good
practice is to define one property definitions file for each source file or major directory
in the service or utility.  For example, the **security** service has several, and by naming them
with names corresponding to the section of the source tree where they are used, it is easy to
locate the usage if necessary.  For a further example, if one of the property definition files
was named `troubleStatePropertyDefinitions.json`, it would correspond to code in the `src/trouble` directory
in **security** service.

The structure of each entry in the property definitions file is as follows:

~~~
[
    {
        "propertyName": "<name_of_first_property>",
        "propertyType": "<type_of_first_property>",
        "minValue":     "<minimum_numeric_value_of_first_property>",
        "maxValue":     "<maximum_nuermic_value_of_first_property>",
        "enumValues":   "<list_of_allowed_values_of_first_property>"
    },
    {
        "propertyName": "<name_of_second_property>",
        "propertyType": "<type_of_second_property>",
        "minValue":     "<minimum_numeric_value_of_second_property>",
        "maxValue":     "<maximum_nuermic_value_of_second_property>",
        "enumValues":   "<list_of_allowed_values_of_second_property>"
    }
]
~~~

The `propertyName` and `propertyType` values are required. The others are optional,
based on the `propertyType`, with the exception that if the `propertyType` is `enum`
then the `enumValues` is required.

The property types supported are as follows:

* "string" -- represents any string value
* "boolean" -- represents a boolean value that must be `true` or `false`
* "uint64" -- represents a numeric unsigned 64 bit integer value
* "uint32" -- represents a numeric unsigned 32 bit integer value
* "uint16" -- represents a numeric unsigned 16 bit integer value
* "uint8" -- represents a numeric unsigned 8 bit integer value
* "int64" -- represents a numeric signed 64 bit integer value
* "int32" -- represents a numeric signed 32 bit integer value
* "int16" -- represents a numeric signed 16 bit integer value
* "int8" -- represents a numeric signed 8 bit integer value
* "enum" -- represents an enumerated value
* "url" -- represents a string value that is a URL
* "urlSet" -- represents a set of string values that are URLs

For the `numeric` values listed above, the `minValue` and `maxValue` values default
to the entire range of possible values for that type.  The `minValue` and/or `maxValue`
values may be defined to restrict the allowed values.

For the `boolean` value listed above, more than just `true` and `false` are supported,
due to some legacy systems supplying these values in other formats.  The possible values
for `boolean` typed properties are:
* `false`
* `true`
* `0`
* `1`
* `no`
* `yes`


For the `enumValues` definition, the list of possible values must be a json array of
the supported values for the `enum`; for example:

~~~
    "enumValues":  ["true","false","dev"]
~~~

Currently, there is no validation provided for the `url` and `urlSet` values.

At build time, all of the property definition files found in all of the `propDefs`
directories are joined together into a single property definitions file that lives
on the device at `/vendor/etc/propertyTypeDefs.json`.  The **props** service looks
in that file for the property definitions in order to establish the property parsing
and validation rules for each property.  Any property that is not defined there will
not be validated, and will be allowed to take on any possible value.
