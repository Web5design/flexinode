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

  static $recursion = 0;

  // If the field collection item form contains another field collection,
  // we might ran into a recursive loop. Prevent that.
  if ($recursion++ > 3) {
    drupal_set_message(t('The flexinode item form has not been embedded to avoid recursive loops.'), 'error');
    return $element;
  }
  $field_parents = $element['#field_parents'];
  $field_name = $element['#field_name'];
  $language = $element['#language'];

  // Nest the flexifield entity form in a dedicated parent space,
  // by appending [field_name, langcode, delta] to the current parent space.
  // That way the form values of the field collection item are separated.
  $parents = array_merge($field_parents, array($field_name, $language, $delta));

  $element += array(
    '#element_validate' => array('flexinode_validate_element'),
    '#parents' => $parents,
  );

  if ($field['cardinality'] == 1) {
    $element['#type'] = 'fieldset';
  }

  $field_state = field_form_get_state($field_parents, $field_name, $language, $form_state);


  if (isset($field_state['flexinode'][$delta])) {
    $old_flexinode = $field_state['flexinode'][$delta]['flexinode'];
    $old_flexinode_type = $field_state['flexinode'][$delta]['flexinode_type'];
    $first_time = false;
  }
  else {
    $first_time = true;
    $old_type_id = isset($items[$delta]['type']) ? $items[$delta]['type'] : NULL;
    if ($old_type_id) {
      #TODO: Remove the check when the rest is working (type should never be set without flid)
      $old_item_flid = isset($items[$delta]['flid'])  ? $items[$delta]['flid'] : NULL;

      $create_clone = abs($old_item_flid) != $old_item_flid;
      $old_item_flid = '' . abs($old_item_flid);
      $old_flexinode = flexinode_field_get_entity($old_item_flid, $old_type_id);
      if ($create_clone) {
        $old_flexinode->flid = NULL;
      }

      $old_flexinode_type = entity_object_load($old_type_id, 'flexinode_type');
    }
    else {
      $old_flexinode = null;
      $old_flexinode_type = null;
    }
  }

  $element['#element_validate'] = array('flexinode_validate_element');

  $element['type'] = array(
    '#type' => 'select',
    '#language' => $langcode,
    '#required' => FALSE,
    '#options' => flexinode_get_flexinodetypes($instance),
    '#default_value' => empty($old_flexinode_type) ? NULL : $old_flexinode_type->id,

    '#ajax' => array(
      'callback' => 'flexinode_widget_ajax_callback',
       'wrapper' => 'flexifield-content-' . $delta,
    ),
  );

  $element['container'] = array(
    '#type' => 'container',
    '#prefix' => '<div id="flexifield-content-' . $delta .   '">',
    '#suffix' => '</div>',
    '#parents' => $parents,
  );


  $form_values = drupal_array_get_nested_value($form_state['input'], $element['#parents']);


  if (!$first_time && isset($form_values['type']) && !empty($form_values['type'])) {
    $submitted_flexinode_type = entity_object_load($form_values['type'], 'flexinode_type');
    if (!is_object($submitted_flexinode_type)) {
      $submitted_flexinode = null;
    }
    elseif (empty($old_flexinode_type) || ($old_flexinode->type != $submitted_flexinode_type->type)) {
      $submitted_flexinode = flexinode_field_get_entity(NULL, $submitted_flexinode_type->type);
    }
    else {
      $submitted_flexinode = $old_flexinode;
    }
  }
  else if (!$first_time) {
    $submitted_flexinode = null;
    $submitted_flexinode_type = null;
  }
  else {
    $submitted_flexinode = $old_flexinode;
    $submitted_flexinode_type = $old_flexinode_type;
  }


  // Put our entity in the form state, so FAPI callbacks can access it.
  $field_state['flexinode'][$delta] = array(
    'flexinode' => $submitted_flexinode,
    'flexinode_type' => $submitted_flexinode_type
  );
  field_form_set_state($field_parents, $field_name, $language, $form_state, $field_state);

  if (!empty($submitted_flexinode)) {
    field_attach_form('flexinode', $submitted_flexinode, $element['container'], $form_state, $language);
  }

  $recursion--;
  return $element;

}


function flexinode_widget_ajax_callback($form, $form_state) {
  $triggering_element = $form_state['triggering_element'];
  $path = $triggering_element['#array_parents'];
  array_pop($path);
  $flexifield_form  = drupal_array_get_nested_value($form, $path);
  return $flexifield_form['container'];
}

/**
 * Flexinode form element validation.
 */
function flexinode_validate_element(&$element, &$form_state) {
  $field = field_widget_field($element, $form_state);
  $field_parents = $element['#field_parents'];
  $field_name = $element['#field_name'];
  $language = $element['#language'];

  $field_state = field_form_get_state($field_parents, $field_name, $language, $form_state);
  $flexinode = $field_state['flexinode'][$element['#delta']]['flexinode'];
  $flexinode_type = $field_state['flexinode'][$element['#delta']]['flexinode_type'];

  if (!empty($flexinode)) {
    // Attach field API validation of the embedded form.
    field_attach_form_validate('flexinode', $flexinode, $element['container'], $form_state);
  }

  if ($form_state['submitted'] && !form_get_errors()) {

    if (!empty($flexinode)) {
      field_attach_submit('flexinode', $flexinode, $element['container'], $form_state);
    }

    // Set the _weight if it is a multiple field.
    if (isset($element['_weight']) && ($field['cardinality'] > 1 || $field['cardinality'] == FIELD_CARDINALITY_UNLIMITED)) {
      $item['_weight'] = $element['_weight']['#value'];
    }
    // Put the flexinode item in $item['flid'], so it is saved with
    // the host entity via hook_field_presave() / field API if it is not empty.
    // @see flexinde_field_presave()

    if ($flexinode) {
      $item['flid'] = $flexinode;
      $item['type'] = $flexinode_type->id;
    }
    else {
      $item['type'] = $item['flid'] = null;
    }
    form_set_value($element, $item, $form_state);
  }
  return;
}

/**
 * Implements hook_field_presave().
 */
function flexinode_field_presave($entity_type, $entity, $field, $instance, $langcode, &$items) {

  $new_items = array();

  foreach ($items as $item) {

    if (isset($item['flid']) && is_object($item['flid'])) {

      $flexinode = $item['flid'];
      $flexinode->is_new = isset($flexinode->is_new) ? $flexinode->is_new : 0;
      if ($flexinode->is_new) {
        $flexinode->created = time();
      }
      $flexinode->changed = time();
      $flexinode->save();
      $item['flid'] = $flexinode->flid;
      $new_items[] = $item;
    }
  }
  $items = $new_items;

  if (!property_exists($entity, 'original')) {
    return;
  }

  $original_items = field_get_items($entity_type,$entity->original,$field['field_name'],$langcode);

  $items_to_delete = array();
  if (!empty($original_items)) {
    foreach ($original_items as $original_item) {
      $found = false;
      foreach ($items as $item) {
        if ($item['flid'] == $original_item['flid']) {
          $found = true;
          break;
        }
      }
      if (!$found) {
        $items_to_delete[] = $original_item;
      }
    }
  }

  foreach ($items_to_delete as $item_to_delete) {
    $flexinode = flexinode_field_get_entity($item_to_delete['flid']);
    $flexinode->delete();
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
    if (isset($item['flid'])) $flids[] = $item['flid'];
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



function flexinode_field_prepare_translation($entity_type, $entity, $field, $instance, $langcode, &$items, $source_entity, $source_langcode)  {
  foreach ($items as &$item) {
    $item['flid'] = -$item['flid'];
  }
}
