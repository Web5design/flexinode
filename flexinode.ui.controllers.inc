<?php
/**
 * @file
 * Contains both entityUIController derived classes for editing flexinode and
 * flexinode-type entities.
 *
 * @author : René Bakx (rene@71media.net)
 */

/**
 * UI controller for the flexinode/
 *
 * Not that much to control, since a flexinode 100% dependend on its host entity.
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




/**
 * UI controller for the flexinode_type.
 *
 * Creates the UI needed to add fields to a single flexinode_type
 */
class FlexinodeTypeUIController extends EntityDefaultUIController {

    /**
     * Overrides hook_menu() defaults.
     */
    public function hook_menu() {
        $items = parent::hook_menu();
        $items[$this->path]['description'] = 'Manage flexinode types';
        foreach ($items as &$item){
          if (isset($item['access callback'])){
            $item['access callback'] = 'flexinode_type_access';
            $item['access arguments']  = array('administer flexinodes');
          }
        }
        return $items;
    }

    public function overviewTable($conditions = array()) {
        $entities = entity_load($this->entityType);
        ksort($entities);
        $rows = array();
        foreach ($entities as $entity) {
            $rows[] = $this->overviewTableRow($conditions, entity_id($this->entityType, $entity), $entity);
        }

        $header = array(t('Label'));
        if (!empty($this->entityInfo['exportable'])) {
            $header[] = t('Status');
        }
        // Add operations with the right colspan.
        $field_ui = !empty($this->entityInfo['bundle of']) && module_exists('field_ui');
        $exportable = !empty($this->entityInfo['exportable']);
        $colspan = 3;
        $colspan = $field_ui ? $colspan + 2 : $colspan;
        $colspan = $exportable ? $colspan + 1 : $colspan;
        $header[] = array('data' => t('Operations'), 'colspan' => $colspan);
        $render = array(
            '#theme' => 'table',
            '#header' => $header,
            '#rows' => $rows,
            '#empty' => t('No flexinode types available. <a href="@link">Add flexinode type</a>.', array('@link' => url('admin/structure/flexinodes/add'))),
        );
        return $render;
    }
}

/**
 * Generates the model type editing form.
 */
function flexinode_type_form($form, &$form_state, $flexinode_type, $op = 'edit') {
    if ($op == 'clone') {
        $flexinode_type->label .= ' (cloned)';
        $flexinode_type->type = '';
    }

    $form['label'] = array(
        '#title' => t('Label'),
        '#type' => 'textfield',
        '#default_value' => $flexinode_type->label,
        '#description' => t('The human-readable name of this model type.'),
        '#required' => TRUE,
        '#size' => 30,
    );
    // Machine-readable type name.
    $form['type'] = array(
        '#type' => 'machine_name',
        '#default_value' => isset($flexinode_type->type) ? $flexinode_type->type : '',
        '#maxlength' => 32,
        '#machine_name' => array(
            'exists' => 'flexinode_get_types',
            'source' => array('label'),
        ),
        '#description' => t('A unique machine-readable name for this model type. It must only contain lowercase letters, numbers, and underscores.'),
    );

    $form['data']['#tree'] = TRUE;

    $form['actions'] = array('#type' => 'actions');
    $form['actions']['submit'] = array(
        '#type' => 'submit',
        '#value' => t('Save'),
        '#weight' => 40,
    );
    if (module_exists('field_ui')) {
        $form['actions']['save_continue'] = array(
            '#type' => 'submit',
            '#value' => t('Save and add fields'),
            '#weight' => 45,
        );
    }
    return $form;
}


/**
 * Form API submit callback for the type form.
 */
function flexinode_type_form_submit(&$form, &$form_state) {
    $flexinode_type = entity_ui_form_submit_build_entity($form, $form_state);
    $flexinode_type->save();
    if ($form_state['triggering_element']['#parents'][0] === 'save_continue') {
        $form_state['redirect'] = 'admin/structure/flexinodes/manage/' . $flexinode_type->type . '/fields';
    }
    else {
        $form_state['redirect'] = 'admin/structure/flexinodes/manage/';
    }
}

/**
 * Form API submit callback for the delete button.
 */
function flexinode_type_form_submit_delete(&$form, &$form_state) {
    $form_state['redirect'] = 'admin/structure/flexinodes/manage/' . $form_state['type']->type . '/delete';
}




