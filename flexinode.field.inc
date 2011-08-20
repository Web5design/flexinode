<?php
/**
 * @file
 * Provides the widget to embed a flexinode into a host entity.
 *
 * @author : René Bakx (rene@71media.net)
 */


/**
 * Implements hook_field_info().
 *
 * Hook in the new flexifield content type
 */

function flexinode_field_info() {
  $fields = array();
  $fields['flexinode'] = array(
    'label' => t('Flexinode'),
    'description' => t('This field internaly embeds the edit form from a flexinode in a host entity'),
    'settings' => array('referenceable_types' => array()),
    'default_widget' => 'flexifield_widget',
    'default_formatter' => 'flexinode_default',
    'property_type' => 'flexinode',
  );
  return $fields;
}

/**
 * Implements hook_field_formatter_info().
 */
function flexinode_field_formatter_info() {
  return array(
    'flexinode_default' => array(
      'label' => t('Default'),
      'field types' => array('flexinode'),
    ));
}

/**
 * Implements hook_schema().
 * Defines storage for flexifield in host entity
 */
function flexinode_field_schema($field) {
  if ($field['type'] == 'flexinode') {
    $columns = array(
      'flid' => array(
        'type' => 'int',
        'unsigned' => TRUE,
        'not NULL' => TRUE,
      ),
      'type' => array(
        'type' => 'int',
        'unsigned' => TRUE,
        'not NULL' => FALSE,
      )
    );
    return array(
      'columns' => $columns,
      'indexes' => array('flid' => array('flid')),
    );
  }
}

/**
 * Implement a settings form for the widget in the host entity
 */

function flexinode_field_settings_form($field, $instance, $has_data) {
  if ($field['type'] == 'flexinode') {
    $options = array();
    $available_types = flexinode_get_types();
    foreach ($available_types as $type) {
      $options += array($type->id => check_plain($type->label));
    }
    $settings = $instance['settings'];
    $form = array();
    $form['referenceable_types'] = array(
      '#type' => 'checkboxes',
      '#title' => t('Content types that can be referenced'),
      '#multiple' => TRUE,
      '#default_value' => is_array($settings['referenceable_types'])
              ? $settings['referenceable_types'] : array(),
      '#options' => $options,
      '#disabled' => $has_data,
    );
    return $form;
  }
}

function flexinode_field_instance_settings_form($field, $instance) {
  $form = array();
  $form['text'] = array(
    '#type' => 'text',
  );
}

/**
 * Implements hook_is_empty().
 */
function flexinode_field_is_empty($item, $field) {
  if (!isset($item['type'])) {
    return true;
  }
  return false;
}

/**
 * Implements hook_widget_info().
 * @return array
 */
function flexinode_field_widget_info() {
  return array(
    'flexinode_field_widget' => array(
      'label' => t('Embedded'),
      'description' => t('Show the entry form for a widget'),
      'field types' => array('flexinode'),
      'settings' => array('referenceable_types' => array()),
      'behaviors' => array(
        'multiple values' => FIELD_BEHAVIOR_DEFAULT,
        'default value' => FIELD_BEHAVIOR_NONE, // no default values for this
      ),
    )
  );
}

/**
 * The actual widget form itself, creates nothing but a container with #process and #element_validate
 * and a select element which triggers the ajax as being set in the #process of the container
 *
 * If the form was not submitted in any kind of way, the initial state of the selector is set as
 * determined by the $items[$delta].
 */
function flexinode_field_widget_form(&$form, &$form_state, $field, $instance, $langcode, $items, $delta, $element) {
  $return = NULL;
  if ($instance['widget']['type'] == 'flexinode_field_widget') {
    $element_name = 'fc_' . $field['field_name'] . '_' . $delta;
    $element[$element_name] = array(
      '#field_name' => $element_name,
      '#type' => 'container',
      '#title' => t('flexifield item'),
      '#tree' => TRUE,
      '#language' => $langcode,
      '#process' => array('flexinode_process_element'), // this is where the subform is attached
      '#element_validate' => array('flexinode_validate_element'), // this where the subform get's saved.
    );

    $element[$element_name]['type'] = array(
      '#type' => 'select',
      '#language' => $langcode,
      '#required' => FALSE,
      '#options' => flexinode_get_flexinodetypes($field),
      '#loaded_value' => isset($items[$delta]['type']) ? $items[$delta]['type'] : NULL,
    );

    $element[$element_name]['flid'] = array(
      '#type' => 'hidden',
      '#value' => isset($items[$delta]['flid']) ? $items[$delta]['flid'] : NULL,
    );
    $return = $element;
  }
  return $return;
}

/**
 * The AJAX callback, replace's the form['content'] with the one generated
 * in flexinode_process_element.
 */
function flexinode_widget_ajax_callback($form, &$form_state) {
  $element = $form_state['triggering_element'];
  array_pop($element['#array_parents']);
  $subform = drupal_array_get_nested_value($form, $element['#array_parents']);
  return $subform['content'];
}

/**
 * Generates the embedded form based on the type set in form_state['input'] or
 * on the type if the record is being edited.
 *
 * @param $element array containing the element currently processed
 * @param $form_state array with form_state;
 * @param $form array actually this the duplicate form from $form_state[]
 * @return array
 */
function flexinode_process_element($element, &$form_state, $form) {
  $target_element = 'ajax_' . $element['#field_name'];
  // placeholder for the new embedded form
  $element['content'] = array(
    '#prefix' => '<div id="' . $target_element . '">',
    '#suffix' => '</div>',
    '#field_name' => 'content',
    '#type' => 'container',
    '#tree' => TRUE,
    '#language' => $element['#language'],
    '#parents' => $element['#parents'],
  );

  // add AJAX to the dropdown selector that replaces the above created container.
  $element['type']['#ajax'] = array(
    'callback' => 'flexinode_widget_ajax_callback',
    'wrapper' => $target_element,
    'method' => 'replace',
    'effect' => 'slide',
    'progress' => array('type' => 'throbber'),
  );
  $flexitype_form_state = drupal_array_get_nested_value($form_state['input'], $element['#parents']);
  $flexinode_type = false;

  $type = false;
  if (isset($flexitype_form_state['type'])) {
    if (is_numeric($flexitype_form_state['type'])) {
      $type = $flexitype_form_state['type'];
    }
  }
  elseif (isset($element['type']['#loaded_value'])) {
    $type = $element['type']['#loaded_value'];
  }
  $flid = isset($element['flid']['#value']) ? $element['flid']['#value'] : null;
  if ($type) {
    $element['type']['#default_value'] = $type;
    $flexinode_type = entity_object_load($type, 'flexinode_type');
    $flexinode_type = $flexinode_type->type;
    $flexinode = flexinode_field_get_entity($flid, $flexinode_type);
    if ($flexinode->type != $flexinode_type) {
      $flexinode = flexinode_field_get_entity(NULL, $flexinode_type);
    }
    field_attach_form('flexinode', $flexinode, $element['content'], $form_state, $element['#language']);
  }
  return $element;
}

/**
 * Implements hook_widget_validate().
 *
 * If the form is submitted, filter out the flexinodes, save them and set
 * the value according to flexinode_field_schema();
 *
 */
function flexinode_validate_element(&$element, &$form_state) {
  /* Save the flexinode if the form was submitted and not on the Add other button */
  if ($form_state['submitted'] && $form_state['triggering_element']['#parents'][0] != $element['#parents'][0]) {
    $form_values = drupal_array_get_nested_value($form_state['input'], $element['#parents']);
    $element_weight = (isset($form_values['_weight'])) ? $form_values['_weight'] : 0;
    $flexinode_type = false;
    $flexinode = false;
    if (!empty($form_values['type'])) {
      $flexinode_type = entity_object_load($form_values['type'], 'flexinode_type');
    }
    /* Check if there is a current flexinode, and if the flexinode is of the same type as the user selected
     * or if the user selected none, in both cases delete current flexinode
    */
    if (isset($form_values['flid']) && is_numeric($form_values['flid'])) {
      $flexinode = flexinode_field_get_entity($form_values['flid']);
      if (!is_object($flexinode_type)) {
        $flexinode->delete();
      }
      elseif ($flexinode->type != $flexinode_type->type) {
        $flexinode->delete();
        $flexinode = flexinode_field_get_entity(NULL, $flexinode_type->type);
      }
    }
      /**
       * User created a new flexinode.
       */
    elseif ($flexinode_type) {
      $flexinode = flexinode_field_get_entity(NULL, $flexinode_type->type);
    }
    /**
     * If there is a flexinode, save it and attach the newly created type & flid to the form_state.
     */
    if (is_object($flexinode)) {
      $flexinode->submit($element['content'], $form_state);
      if ($flexinode_type && $flexinode->flid) {
        $values = array('_weight' => $element_weight, 'type' => $flexinode_type->id, 'flid' => $flexinode->flid);
        array_pop($element['#parents']);
        form_set_value($element, $values, $form_state);
      }
      else {
        form_error($element['field_name'], t('Could not save !fieldname'), array('!fieldname' => $element['field_name']));
      }
    }
  }
}


/**
 * create or returns the entity for the embedded form.
 */
function flexinode_field_get_entity($flid = NULL, $type = NULL) {
  $return = FALSE;
  if (is_numeric($flid)) {
    $return = flexinode_load_flexinode($flid);
  }
  elseif (isset($type)) {
    $return = entity_get_controller('flexinode')->create(array('type' => $type));
  }
  return $return;
}


function flexinode_field_formatter_view($flexinode_type, $flexinode, $field, $instance, $langcode, $items, $display) {
  $element = array();
  foreach ($items as $delta => $item) {
    if ($flexinode = flexinode_field_get_entity($item['flid'])) {
      $element[$delta] = $flexinode->view();
    }
  }
  return $element;
}


/**
 * Implements hook_field_delete().
 */
function flexinode_field_delete($entity_type, $entity, $field, $instance, $langcode, &$items) {
  $flids = array();
  foreach ($items as $item) {
    if (isset($item['flid'])) $flids += $item['flid'];
  }
  flexinode_delete_multiple($flids);
}

/**
 * Returns available flexinode_types for a given field.
 * @param $field
 * @return array
 */
function flexinode_get_flexinodetypes($field) {
  $name = '_flexinodetype_' . $field['field_name'];
  $available_flexinode_types = drupal_static($name);
  if (!$available_flexinode_types) {
    $available_flexinode_types = array(NULL => t('none'));
    $all_flexinode_types = flexinode_get_types(NULL, TRUE);
    foreach ($all_flexinode_types as $id => $flexinode_type) {
      if (in_array($id, array_values($field['settings']['referenceable_types']))) {
        $available_flexinode_types[$id] = $flexinode_type->label;
      }
    }
    drupal_static($name, $available_flexinode_types);
  }
  return $available_flexinode_types;
}