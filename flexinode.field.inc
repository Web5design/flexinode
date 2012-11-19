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
 * Implements hook_field_formatter_info().
 */
function flexinode_field_formatter_info() {
  return array(
    'flexinode_default' => array(
      'label' => t('Default'),
      'field types' => array('flexinode'),
    ),
    'flexinode_inherit' => array(
      'label' => t('Inherit'),
      'field types' => array('flexinode'),
      'settings' => array('displaymode' => array('default' => array())),
    ),
  );
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
 * Implements the hook_fields_settings_form().
 */

function flexinode_field_settings_form($field, $instance, $has_data) {
  return array(
    '#type' => 'markup',
    '#prefix' => t('Flexinode referrences are per instance and can be set on the edit tab'),
  );
}

/**
 * Implements the hook_fields_instance_settings_form().
 */
function flexinode_field_instance_settings_form($field, $instance) {
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
      '#default_value' => (isset($settings['referenceable_types']))
              ? $settings['referenceable_types'] : array(),
      '#options' => $options,
    );
    return $form;
  }
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
      '#options' => flexinode_get_flexinodetypes($instance),
      '#loaded_value' => isset($items[$delta]['type']) ? $items[$delta]['type'] : NULL,
    );

    if ($field['cardinality'] == 1) {
      $element[$element_name]['type']['#title'] = $instance['label'];
    }

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
    array_pop($element['#parents']);
    $form_values = drupal_array_get_nested_value($form_state['input'], $element['#parents']);
    $weight = (isset($form_values['_weight'])) ? $form_values['_weight']
            : $element['#weight'];
    $form_values = $form_values[$element['#field_name']];
    /**
     * Check if there is a current flexinode, and if the flexinode is of the same type as the user selected
     * or if the user selected none, in both cases delete current flexinode
     */
    $flexinode_type = false;
    $flexinode = false;
    if (!empty($form_values['type'])) {
      $flexinode_type = entity_object_load($form_values['type'], 'flexinode_type');
    }

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
        $values = array('_weight' => $weight, 'type' => $flexinode_type->id, 'flid' => $flexinode->flid);
        form_set_value($element, $values, $form_state);
      }
      else {
        form_error($element['field_name'], t('Could not save !fieldname'), array('!fieldname' => $element['field_name']));
      }
    }
  }
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
 * create or returns the entity to be embedded in the host entity
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


function flexinode_field_formatter_settings_form($field, $instance, $view_mode, $form, &$form_state) {
  $display = $instance['display'][$view_mode];
  $referencables = entity_load('flexinode_type', $instance['settings']['referenceable_types']);
  if ($display['type'] == 'flexinode_inherit') {
    $options = array();
    foreach ($referencables as $referencable) {
      $modes = field_view_mode_settings('flexinode', $referencable->type);
      $viewmodes = array();
      foreach ($modes as $name => $settings) {
        if ($settings['custom_settings']) {
          $viewmodes[$name] = $name;
        }
      }
      $form['updated'] = array(
        '#type' => 'hidden',
        '#value' => true,
      );
      $form['displaymode'][$view_mode][$referencable->type] = array(
        '#type' => 'select',
        '#options' => $viewmodes,
        '#title' => check_plain($referencable->label),
      );
    }
  }
  return $form;
}


/**
 * Implements hook_field_formatter_settings_summary().
 *
 * And since i could not find another way without creating a shitload of code
 * the above created formatter sneaky inserts a UPDATED into $instance
 * so I could save the settings with field_instance_update();
 */
function flexinode_field_formatter_settings_summary($field, $instance, $view_mode) {
  $display = $instance['display'][$view_mode];
  $settings = $display['settings'];
  if ($display['type'] == 'flexinode_inherit') {
    $referencables = entity_load('flexinode_type', $instance['settings']['referenceable_types']);
    foreach ($referencables as $referencable) {
      $lookup[$referencable->type] = $referencable->label;
    }
    if (isset($settings['updated'])) {
      unset($settings['updated']);
      field_update_instance($instance);
    }
    $summary = '';
    // TODO, move to decent theme function ;)
    foreach ($settings['displaymode'][$view_mode] as $flexinode => $mode) {
      $summary .= $lookup[$flexinode] . ' -> ' . $mode . '<br>';
    }
    return $summary;
  }
}

/**
 * Implements hook_field_formatter_view().
 */
function flexinode_field_formatter_view($flexinode_type, $flexinode, $field, $instance, $langcode, $items, $display) {
  $element = array();
  $display_modes = array();
  if (isset($display['settings']['displaymode'])) {
    $display_modes = $display['settings']['displaymode']['default']; // TODO check with custom display modes later!
  }
  foreach ($items as $delta => $item) {
    if ($flexinode = flexinode_field_get_entity($item['flid'])) {
      switch ($display['type']) {
        case 'flexinode_default':
        default:
          $element[$delta] = $flexinode->view();
          break;
        case 'flexinode_inherit':
          $node = $flexinode->view();
          $node['#view_mode'] = $display_modes[$node['#bundle']];
          $element[$delta] = $node;
          break;
      }
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