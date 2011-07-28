<?php
// $Id: Exp $

/**
 * @file mfx.field.inc
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
 * @return array
 */

function mfx_field_info() {
    $fields = array();
    $fields['flexifield'] = array(
        'label' => t('Flexifield'),
        'description' => t('This field internaly embeds the edit form from a flexinode'),
        'settings' => array('referenceable_types' => array()),
        'default_widget' => 'mfx_flexifield_widget',
        'default_formatter' => 'mfx_flexifield_formatter',
        'property_type' => 'flexinode',
    );
    return $fields;
}

/**
 * Implements hook_schema().
 * Defines storage for flexifield in host entity
 * @param $field
 * @return array
 */
function mfx_field_schema($field) {
    if ($field['type'] == 'flexifield') {
        $columns = array(
            'flid' => array(
                'type' => 'int',
                'unsigned' => TRUE,
                'not null' => TRUE,
            ),
        );
        return array(
            'columns' => $columns,
            'indexes' => array('flid' => array('flid')),
        );
    }
}

/**
 * This form sets the parameters on a field level in the host entity.
 */
function mfx_field_settings_form($field, $instance, $has_data) {
    if ($field['type'] == 'flexifield') {
        $available_types = mfx_flexinode_get_types();
        $options = array();
        foreach ($available_types as $type) {
            $options += array($type->type => check_plain($type->label));
        }
        unset($available_types[$instance['bundle']]); // no self references
        $settings = $field['settings'];
        $form = array();
        $form['referenceable_types'] = array(
            '#type' => 'checkboxes',
            '#title' => t('Content types that can be referenced'),
            '#multiple' => TRUE,
            '#default_value' => is_array($settings['referenceable_types']) ? $settings['referenceable_types']
                    : array(),
            '#options' => $options,
            '#disabled' => $has_data,
        );
        return $form;
    }
}

function mfx_field_is_empty($item, $field) {
    return false;
}

/**
 * Implements hook_widget_info().
 * @return array
 */
function mfx_field_widget_info() {
    return array(
        'mfx_flexifield_widget' => array(
            'label' => t('Flexifield widget'),
            'description' => t('Show the entry form for a widget'),
            'field types' => array('flexifield'),
            'settings' => array(),
            'behaviors' => array(
                'multiple values' => FIELD_BEHAVIOR_DEFAULT,
                'default value' => FIELD_BEHAVIOR_DEFAULT, // no default values for this
            ),
        )
    );
}

/**
 * The actuall widget form itself, creates a $delta indexed container with a dropdown to select the
 * flexinode to embed. This container is replaced trough a AJAX call with a new subform
 * containing both the selector and the embedded form for the flexinode based on the $delta of the selector.
 */
function mfx_field_widget_form(&$form, &$form_state, $field, $instance, $langcode, $items, $delta, $element) {
    $return = false;
    if ($instance['widget']['type'] == 'mfx_flexifield_widget') {
        if (!in_array('mfx_flexifield_widget_submit', $form['#submit'])) {
            $form['#submit'][] = 'mfx_flexifield_widget_submit';
        }
        $elementName = 'flexi_container_' . $field['field_name'];
        $targetElement = 'ajax_' . $elementName . '_' . $delta;
        $field_name = $element['#field_name'];
        // get available flexinodes for this field.
        $available_flexinode_types = array_map('check_plain', array_merge(array(null => t('none')), $field['settings']['referenceable_types']));
        foreach ($available_flexinode_types as $key => $reference) {
            if ($reference == '0') {
                unset($available_flexinode_types[$key]);
            }
        }
        // build the actual widget element.
        $element[$elementName] = array(
            '#field_name' => $elementName,
            '#type' => 'container',
            '#title' => t('flexifield item'),
            '#weight' => $delta,
            '#delta' => $delta,
            '#tree' => true,
            '#language' => $langcode,
            '#element_validate' => array('mfx_flexifield_widget_validate'),
            '#prefix' => '<div id="' . $targetElement . '">',
            '#suffix' => '</div>'
        );

        $element[$elementName]['select'] = array(
            '#delta' => $delta,
            '#type' => 'select',
            '#required' => false,
            '#default_value' => isset($items[$delta]['type']) ? $items[$delta]['type'] : NULL,
            '#options' => $available_flexinode_types,
            '#ajax' => array(
                'callback' => 'mfx_flexifield_widget_trigger_subform_ajax',
                'wrapper' => $targetElement,
                'method' => 'replace',
                'effect' => 'slide',
                'progress' => array('type' => 'throbber'),
            )
        );
        $parents = array_merge($element['#field_parents'], array($field_name, $langcode, $delta, $elementName, 'content'));
        // on AJAX callback add the embedded form into the widget for the current $delta
        $ajaxtriggered = (isset($form_state['triggering_element'])) ? true : false;
        $flexinodetype = (isset($form_state['triggering_element']["#value"])) ? $form_state['triggering_element']["#value"] : NULL;
        $triggeringdelta = (isset($form_state['triggering_element']['#delta'])) ? $form_state['triggering_element']['#delta'] : NULL;
        if ($triggeringdelta == $delta) {
            $flexinode = mfx_field_get_entity($items[$delta], $flexinodetype);
            if (is_object($flexinode)) {
                $element[$elementName]['content'] = array(
                    '#field_name' => 'content',
                    '#delta' => $delta,
                    '#type' => 'container',
                    '#tree' => true,
                    '#language' => $langcode,
                    '#field_parents' => $parents,
                    '#parents' => $parents,

                );
                field_attach_form('flexinode', $flexinode, $element[$elementName]['content'], $form_state, $langcode);
                // store a copy of the entire widget to be restored in the form_alter hook below.
                $form_state['#flexinodes'][$field_name][$delta] = array('entity' => $flexinode, 'subform' => $element);
            } elseif ($ajaxtriggered && empty($flexinodetype)) {
                // object was cleared, so it must be removed from form_state too.
                unset($form_state['#flexinodes'][$field_name][$delta]);
            }
        }
        $return = $element;
    }
    return $return;
}

/**
 * The actual AJAX callback, replaces the entire above created flexi_container_{field_name}
 */
function mfx_flexifield_widget_trigger_subform_ajax($form, &$form_state) {
    $element = $form_state['triggering_element'];
    array_pop($element['#parents']);
    $subform = drupal_array_get_nested_value($form, $element['#parents']);
    return $subform;
}

/**
 * implements hook_widget_validate().
 *
 * Besides validating, the embedded Flexinode is being saved to the database.
 */
function mfx_flexifield_widget_validate($element, &$form_state, $form) {
    if ($form_state['submitted']) {
        $delta = $element['#delta'];
        $language = $element['#language'];
        $fieldname = reset($element['#parents']);
        /** @var $flexinode Flexinode */
        $flexinode = (isset($form_state['#flexinodes'][$fieldname][$delta]['entity'])) ? $form_state['#flexinodes'][$fieldname][$delta]['entity'] : false;
        if (is_object($flexinode)) {
            $subform = $form_state['#flexinodes'][$fieldname][$delta]['subform'][$element['#field_name']]['content'];
            $subform['#delta'] = $delta;
            #$subform_values = $form_state['values'][$fieldname][$language][$delta][$element['#field_name']];
            if ($flid = $flexinode->submit($subform, $form_state)) {
                $field_value = array('flid' => $flid);
                array_pop($element['#parents']);
                form_set_value($element, $field_value, $form_state);
            }
        } else {
            // nothing to save.
            unset($form_state['values'][$fieldname][$language][$delta]);
        }
    }

    //
    //
    //    $flexinode = $field_state['entity'];
    //    // Attach field API validation of the embedded form.
    //    field_attach_form_validate('flexinode', $flexinode, $element, $form_state);

    // TODO subform validation!
    //    if (!field_collection_item_is_empty($flexinode_entity) && !empty($element['#field_collection_required_elements'])) {
    //        foreach ($element['#field_collection_required_elements'] as &$elements) {
    //            // Copied from _form_validate().
    //            if (isset($elements['#needs_validation'])) {
    //                $is_empty_multiple = (!count($elements['#value']));
    //                $is_empty_string = (is_string($elements['#value']) && drupal_strlen(trim($elements['#value'])) == 0);
    //                $is_empty_value = ($elements['#value'] === 0);
    //                if ($is_empty_multiple || $is_empty_string || $is_empty_value) {
    //                    if (isset($elements['#title'])) {
    //                        form_error($elements, t('!name field is required.', array('!name' => $elements['#title'])));
    //                    }
    //                    else {
    //                        form_error($elements);
    //                    }
    //                }
    //            }
    //        }
    //    }
}


function mfx_flexifield_widget_submit(&$form, &$form_state) {
    #dpr($form);
}


/**
 * create or returns the entity for the embedded form.
 */
function mfx_field_get_entity(&$item, $type = NULL) {
    if (isset($item['flid'])) {
        dpm('looaaad');
    }
    elseif (!isset($item['entity']) && isset($type)) {
        $item['entity'] = entity_get_controller('flexinode')->create(array('type' => $type));
    }
    return isset($item['entity']) ? $item['entity'] : FALSE;
}

/**
 * Implements hook_form_alter().
 *
 * Drupal uses a static stack to keep track of the current form, however since the form is changed trough
 * AJAX multiple times, that gave some issues. PHP is stateless, and drupal static is not saved anywwere the
 * content of the static is lost between ajax calls. However the form_state is being passed around as
 * refference everywhere and even kept in the form cache, we rebuild the current form by sneeking in
 * the elements within the form_alter so that on submission all the form_state['fields'] should be present.
 *
 */
function mfx_form_alter(&$form, &$form_state) {
    if (isset($form_state['#flexinodes'])) {
        $triggerdelta = false;
        $triggerelement = false;
        if (isset($form_state['triggering_element'])) {
            $triggerdelta = $form_state['triggering_element']['#delta'];
            $triggerelement = $form_state['triggering_element']['#delta']['#parents'][0];
        }
        foreach ($form_state['#flexinodes'] as $element => $parameters) {
            foreach ($parameters as $delta => $field_state) {
                // do not replace the last created AJAX element.
                if ($field_state['subform']['#field_name'] == $triggerelement && $delta == $triggerdelta) continue;
                $language = $field_state['subform']['#language'];
                $field_state['subform']['_weight'] = $form[$element][$language][$delta]['_weight'];
                $form[$element][$language][$delta] = $field_state['subform'];
            }
        }
    }
}


