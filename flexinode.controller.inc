<?php

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
      'archived' => FALSE,
    );

    $flexinode = parent::create($values);
    return $flexinode;
  }


//  /**
//   * Overriding the buldContent function to add entity specific fields
//   */
  public function buildContent($entity, $view_mode = 'full', $langcode = NULL, $content = array()) {
    $content = parent::buildContent($entity, $view_mode, $langcode, $content);
    $content['#theme'] = 'flexinode';
    return $content;
  }

  public function view($entities, $view_mode = 'full', $langcode = NULL, $page = NULL) {
    $content = parent::view($entities, $view_mode, $langcode, $page);
    $content = $content['flexinode'][$entities[0]->flid];
    return $content;
  }
}

