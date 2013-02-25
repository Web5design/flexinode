<?php
class Flexinode extends Entity {
  public $is_new;
  public $flid;
  public $vid;

  public function __construct($values = array()) {
    parent::__construct($values, 'flexinode');
  }


  protected function defaultLabel() {
    if (isset($this->type)) {
      return t('Flexinode') . ' ' . $this->type;
    }
    return t('Flexinode');
  }

  protected function defaultUri() {
    return array('path' => 'flexinode/' . $this->flid);
  }

  public function create(array $values = array()) {
    // Add values that are specific to our Model
    $values += array(
      'flid' => NULL,
      'vid' => NULL,
      'is_new' => TRUE,
      'archived' => FALSE,
    );

    $flexinode = parent::create($values);
    return $flexinode;
  }

}