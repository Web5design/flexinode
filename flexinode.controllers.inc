<?php
/**
 * @file
 * Contains both entityController derived classes for editing flexinode and
 * flexinode-type entities.
 *
 * @author : René Bakx (rene@71media.net)
 */


class flexinodeController extends EntityAPIController {
  public function __construct($entityType) {
    parent::__construct($entityType);
  }


  /**
   * Create a new flexinode
   *
   */
  public function create(array $values = array()) {
    // Add values that are specific to our Model
    $values += array(
      'flid' => '',
      'is_new' => TRUE,
      'created' => '',
      'changed' => '',
    );

    $flexinode = parent::create($values);
    return $flexinode;
  }
}


/**
 * The Controller for Model entities
 */
class flexinodeTypeController extends EntityAPIControllerExportable {
  public function __construct($entityType) {
    parent::__construct($entityType);
  }

  public function create(array $values = array()) {
    $values += array(
      'id' => '',
      'is_new' => TRUE,
    );
    $flexinode_type = parent::create($values);
    return $flexinode_type;
  }

  public function load($ids = array(), $conditions = array()) {
    return parent::load($ids, $conditions);
  }
}