<?php

/**
 * 
 * @file
 * flexinode editing UI.
 *
 * No to much to edit trough normal UI, since the flexinode only excist as
 * a embedded type.
 */

/**
 * UI controller.
 */
class FlexinodeUIController extends EntityDefaultUIController {

  /**
   * Overrides hook_menu() defaults. Main reason for doing this is that
   * there is no menu, flexinodes are hidden from the user at this moment.
   */
  public function hook_menu() {
      return array();
  }
}







