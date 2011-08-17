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
 *
 * TODO actually implement this ;)
 */
function flexinode_field_is_empty($item, $field) {
  return FALSE;
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

    ($field['cardinality'] == '-1') ? $unlimited_fields = true : $unlimited_fields = false;

    $element_name = 'fc_' . $field['field_name'] . '_' . $delta;
    $available_flexinode_types = array(NULL => t('none'));
    $all_flexinode_types = flexinode_get_types(NULL, TRUE);
    foreach ($all_flexinode_types as $id => $flexinode_type) {
      if (in_array($id, array_values($field['settings']['referenceable_types']))) {
        $available_flexinode_types[$id] = $flexinode_type->label;
      }
    }
    $element[$element_name] = array(
      '#field_name' => $element_name,
      '#type' => 'container',
      '#flexi' => isset($items[$delta]) ? $items[$delta] : NULL,
      '#title' => t('flexifield item'),
      '#delta' => $delta,
      '#tree' => TRUE,
      '#weight' => $delta,
      '#language' => $langcode,
      '#process' => array('flexinode_process_element'), // this is where the subform is attached
      '#element_validate' => array('flexinode_validate_element'), // this where the subform get's saved.
    );
    $element[$element_name]['select'] = array(
      '#delta' => $delta,
      '#type' => 'select',
      '#language' => $langcode,
      '#required' => FALSE,
      '#options' => $available_flexinode_types,
    );
    if (!isset($form_state['triggering_element'])) {
      if (isset($items[$delta]['type'])) {
        $element[$element_name]['select']['#default_value'] = $items[$delta]['type'];
        drupal_array_set_nested_value(
          $form_state['input'],
          array($element['#field_name'], $langcode, $delta, $element_name),
          array('select' => $items[$delta]['type'])
        );
      }
    }
    if ((!$unlimited_fields) || ($unlimited_fields && $delta == 0)) {
      $return = $element;
    }
    elseif ((isset($items[$delta]['type'])) || isset($form_state['triggering_element'])) {
      $return = $element;
    }
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
  $element['content'] = array(
    '#prefix' => '<div id="' . $target_element . '">',
    '#suffix' => '</div>',
    '#field_name' => 'content',
    '#delta' => $element['#delta'],
    '#type' => 'container',
    '#tree' => TRUE,
    '#language' => $element['#language'],
    '#parents' => array_merge($element['#parents'], array('content')),
  );
  $element['select']['#ajax'] = array(
    'callback' => 'flexinode_widget_ajax_callback',
    'wrapper' => $target_element,
    'method' => 'replace',
    'effect' => 'slide',
    'progress' => array('type' => 'throbber'),
  );

  $flexitype_form_state = drupal_array_get_nested_value($form_state['input'], $element['#parents']);
  $flexinode_type = false;
  if (isset($flexitype_form_state['select'])) {
    if (!empty($flexitype_form_state['select'])) {
      $flexinode_type = entity_object_load($flexitype_form_state['select'], 'flexinode_type');
      $flexinode_type = $flexinode_type->type;
    }
  }
  $flid = (isset($element['#flexi']['flid'])) ? $element['#flexi']['flid'] : NULL;
  $flexinode = flexinode_field_get_entity($flid, $flexinode_type);
  // if the type has changed, init a new flexinode to attach.
  if ($flexinode->type != $flexinode_type) {
    $flexinode = flexinode_field_get_entity(NULL, $flexinode_type);
  }
  field_attach_form('flexinode', $flexinode, $element['content'], $form_state, $element['#language']);
  return $element;
}

/**
 * Implements hook_widget_validate().
 *
 * If the form is submitted, filter out the flexinodes, save them and set
 * the value according to flexinode_field_schema();
 *
 * TODO Does not validate at this moment, and clean up.. feels to dirty..
 *
 */
function flexinode_validate_element(&$element, &$form_state) {
  if ($form_state['submitted']) {
    // Most likely this was the add more button instead of submit.
    if ($form_state['triggering_element']['#parents'][0] != $element['#parents'][0]) {
      $parents = $element['#parents'];
      array_pop($parents);
      array_pop($parents);
      $parents[] = $element['#delta'];
      // fetch the submited values including the weight..
      $form_values = drupal_array_get_nested_value($form_state['input'], $parents);
      $element_weight = $form_values['_weight'];
      $parents[] = $element['#field_name'];
      // transpose just values needed.
      $form_values = $form_values[$element['#field_name']];
      $flexinode_type = false;
      $flexinode = false;

      if (!empty($form_values['select'])) {
        $flexinode_type = entity_object_load($form_values['select'], 'flexinode_type');
      }
      $update = false;
      if (isset($element['#flexi']['flid'])) {
        $update = true;
        $flexinode = flexinode_field_get_entity($element['#flexi']['flid'], $flexinode_type->type);
      }
      /*
      * If there is no type set by the user, or the type has changed. Delete
      * the current flexinode, to prevent orphaned fields.
      */
      if (is_object($flexinode)) {
        if ((!$flexinode_type) || ($flexinode->type != $flexinode_type->type)) {
          $flexinode->delete();
          $update = false;
        }
      }

      if ($flexinode_type && !$update) {
        $flexinode = flexinode_field_get_entity(NULL, $flexinode_type->type);
      }
      if (is_object($flexinode)) {
        $flexinode->submit($element['content'], $form_state);
        if ($flexinode_type && $flexinode->flid) {
          $values = array('_weight' => $element_weight, 'type' => $flexinode_type->id, 'flid' => $flexinode->flid);
          array_pop($element['#parents']);
          form_set_value($element, $values, $form_state);
        }
        else {
          // this should not happen, perhaps throw error ;)
        }
      }
    }
  }
}


/**
 * create or returns the entity for the embedded form.
 */
function flexinode_field_get_entity($flid = NULL, $type = NULL) {
  $return = FALSE;
  if (isset($flid)) {
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