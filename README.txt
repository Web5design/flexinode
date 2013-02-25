A ENTITY API based implementation of the Drupal 6 flexifield module (http://drupal.org/project/flexifield),

This is not a port, from the above mentioned module, but a new from scratch implementation hence there is no upgrade path available.


Flexinodes are field-able entities that can be embedded into a host entity editing form.

The main difference between this project and field_collection is that flexinodes allow a site builder to define and embed  more then one flexinode type per host form field. And share the flexinodes fields between other entities in the installation. Flexinode is a one to many relation between host and flexinodes while field_collection is a one to one relation between host and field_collection.

More information at this moment can be found on my blog : http://renebakx.nl/65/sneak-peak-into-flexifields-for-drupal-7/

A big thanks to the guys from Freie Formation and leonevers for providing some patches and a even bigger thanks to flink.nl where we used this module in a production environment and forcing me to finally fix this thing.

Changelog:

7.0.0-beta-3:
* Added initial support for revisions with hook_update_X

7.1.0-beta-1
* Almost an entire rewrite of the flexinode system. 
* Added revision support
* Moved around code to create a better abstraction between UI and other logic
* Flexinodes can be embedded in flexinodes.
* Added a delete button to remove a flexinode, this button only appears if needed.
* Added template support.
* Should be updatable from previous versions, but this is not thoroughly tested (backup first!)
* Only show a drop down selector if a user can select more then one type to embedded.
 
