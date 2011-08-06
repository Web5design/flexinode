<?php
// $Id: Exp $

/**
 * @file flexinode.field.inc
 *
 * Provides the embeded field to be used in fieldable entities.
 *
 * @author : René Bakx (rene@71media.net)
 *
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
    'description' => t('This field internaly embeds the edit form from a flexinode'),
    'settings' => array('referenceable_types' => array()),
    'default_widget' => 'mfx_flexifield_widget',
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
        'not null' => TRUE,
      ),
      'type' => array(
        'type' => 'int',
        'unsigned' => TRUE,
        'not null' => FALSE,
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
function mfx_field_settings_form($field, $instance, $has_data) {
  if ($field['type'] == 'flexinode') {
    $options = array();
    $available_types = flexinode_flexinode_get_types();
    foreach ($available_types as $type) {
      $options += array($type->id => check_plain($type->label));
    }
    $settings = $field['settings'];
    $form = array();
    $form['referenceable_types'] = array(
      '#type' => 'checkboxes',
      '#title' => t('Content types that can be referenced'),
      '#multiple' => TRUE,
      '#default_value' => is_array($settings['referenceable_types'])
              ? $settings['referenceable_types']
              : array(),
      '#options' => $options,
      '#disabled' => $has_data,
    );
    return $form;
  }
}

/* Implements hook_is_empty().
* TODO actually implement this ;)
*/
function flexinode_field_is_empty($item, $field) {
  return false;
}

/**
 * Implements hook_widget_info().
 * @return array
 */
function flexinode_field_widget_info() {
  return array(
    'flexinode_flexifield_widget' => array(
      'label' => t('Embedded'),
      'description' => t('Show the entry form for a widget'),
      'field types' => array('flexinode'),
      'settings' => array(),
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
  if ($instance['widget']['type'] == 'flexinode_flexifield_widget') {
    $elementName = 'fc_' . $field['field_name'] . '_' . $delta;
    $available_flexinode_types = array(null => t('none'));
    $available_flexinode_types += array_combine(array_values($field['settings']['referenceable_types']),
                                                array_values(flexinode_flexinode_get_types(null, true)));
    $element[$elementName] = array(
      '#field_name' => $elementName,
      '#type' => 'container',
      '#flexi' => isset($items[$delta]) ? $items[$delta] : null,
      '#title' => t('flexifield item'),
      '#weight' => $delta,
      '#delta' => $delta,
      '#tree' => true,
      '#language' => $langcode,
      '#process' => array('flexinode_process_element'), // this is where the subform is attached
      '#element_validate' => array('flexinode_validate_element'), // this where the subform get's saved.
    );
    $element[$elementName]['select'] = array(
      '#delta' => $delta,
      '#type' => 'select',
      '#language' => $langcode,
      '#required' => false,

      '#options' => $available_flexinode_types,
    );
    if (!isset($form_state['triggering_element'])) {
      $type = (isset($items[$delta]['type'])) ? $items[$delta]['type']
              : null;
      $element[$elementName]['select']['#default_value'] = $type;
      drupal_array_set_nested_value($form_state['input'], array($element['#field_name'], $langcode, $delta, $elementName), array('select' => $type));
    }
    $return = $element;
  }
  return $return;
}

/**
 * The AJAX callback, replace's the form['content'] with the one generated
 * in flexinode_process_element.
 */
function flexinode_flexifield_widget_trigger_subform_ajax($form, &$form_state) {
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
  $targetElement = 'ajax_' . $element['#field_name'];
  $element['content'] = array(
    '#prefix' => '<div id="' . $targetElement . '">',
    '#suffix' => '</div>',
    '#field_name' => 'content',
    '#delta' => $element['#delta'],
    '#type' => 'container',
    '#tree' => true,
    '#language' => $element['#language'],
    '#parents' => array_merge($element['#parents'], array('content')),
  );
  $element['select']['#ajax'] = array(
    'callback' => 'flexinode_flexifield_widget_trigger_subform_ajax',
    'wrapper' => $targetElement,
    'method' => 'replace',
    'effect' => 'slide',
    'progress' => array('type' => 'throbber'),
  );

  $flexitype_form_state = drupal_array_get_nested_value($form_state['input'], $element['#parents']);
  $type = $element['select']['#options'][$flexitype_form_state['select']];
  $flid = (isset($element['#flexi']['flid'])) ? $element['#flexi']['flid']
          : null;
  $flexinode = flexinode_field_get_entity($flid, $type);
  // if the type has changed, init a new flexinode to attach.
  if ($flexinode->type != $type) {
    $flexinode = flexinode_field_get_entity(null, $type);
  }
  field_attach_form('flexinode', $flexinode, $element['content'], $form_state, $element['#language']);
  return $element;
}

/**
 * implements hook_widget_validate().
 *
 * If the form is submitted, filter out the flexinodes, save them and set
 * the value according to flexinode_field_schema();
 *
 * TODO Does not validate at this moment.
 *
 */
function flexinode_validate_element(&$element, &$form_state) {
  if ($form_state['submitted']) {
    // Process the element, so the correct subform is attached. TODO figure out WHY this is needed :(
    $flexinodetype = drupal_array_get_nested_value($form_state['input'], $element['#parents']);
    $type_id = $flexinodetype['select'];
    $flexinodetype = $element['select']['#options'][$type_id];
    $flid = (isset($element['#flexi']['flid'])) ? $element['#flexi']['flid']
            : null;
    $flexinode = flexinode_field_get_entity($flid, $flexinodetype);
    if ($flexinode->type != $flexinodetype) {
      $flexinode = flexinode_field_get_entity(null, $flexinodetype);
    }

    if (!$flid = $flexinode->submit($element['content'], $form_state)) {
      form_set_error('Flexinode', t('There was a problem storing a new flexinode'));
    }
    else {
      array_pop($element['#parents']);
      form_set_value($element, array('type' => $type_id, 'flid' => $flid), $form_state);
    }
    // TODO subform validation!
  }
}


/**
 * create or returns the entity for the embedded form.
 */
function flexinode_field_get_entity($flid = null, $type = NULL) {
  $return = false;
  if (isset($flid)) {
    $return = flexinode_load_flexinode($flid);
  }
  elseif (!isset($item['entity']) && isset($type)) {
    $return = entity_get_controller('flexinode')->create(array('type' => $type));
  }
  return $return;
}


function flexinode_field_formatter_view($flexinode_type, $flexinode, $field, $instance, $langcode, $items, $display) {
  $element = array();
  foreach ($items as $delta => $item) {
    if ($flexinode = flexinode_field_get_entity($item['flid'])) {
      /** @var $flexinode Flexinode */
      $element[$delta] = $flexinode->view();
    }
  }
  return $element;
}