/***************************************************************************
 * PHAST: PHylogenetic Analysis with Space/Time models
 * Copyright (c) 2002-2005 University of California, 2006-2009 Cornell 
 * University.  All rights reserved.
 *
 * This source code is distributed under a BSD-style license.  See the
 * file LICENSE.txt for details.
 ***************************************************************************/

/* $Id: trees.c,v 1.25 2008-11-12 02:07:59 acs Exp $ */

/** \file trees.c 
  Functions for manipulating phylogenetic trees.  Includes functions
  for reading, writing, traversing, and printing.  Trees are
  represented as rooted binary trees with non-negative real branch
  lengths.  
  \ingroup phylo
*/

#include <stdlib.h>
#include <stdio.h>
#include <lists.h>
#include <stringsplus.h>
#include <msa.h>
#include <gff.h>
#include <category_map.h>
#include <getopt.h>
#include <tree_model.h>
#include <fit_em.h>
#include <subst_mods.h>
#include <string.h>
#include <local_alignment.h>
#include <assert.h>
#include <ctype.h>
#include <tree_likelihoods.h>
#include <numerical_opt.h>
#include <sufficient_stats.h>
#include <maf.h>
#include <phylo_fit.h>
#include <stacks.h>
#include <trees.h>
#include <misc.h>

/* initialize phyloFit options to defaults (slightly different
   for rphast).
 */
struct phyloFit_struct* phyloFit_struct_new(int rphast) {
  struct phyloFit_struct *pf = smalloc(sizeof(struct phyloFit_struct));
  pf->msa = NULL;
  pf->output_fname_root = rphast ? NULL : "phyloFit";
  pf->log_fname = NULL;
  pf->reverse_group_tag = NULL;
  pf->root_seqname = NULL;
  pf->subtree_name = NULL;
  pf->error_fname = NULL;
  pf->see_for_help = rphast ? "phyloFit" : "'phyloFit -h'";
  pf->parsimony_cost_fname = NULL;
  pf->msa_fname = NULL;
  pf->subst_mod = REV;
  pf->quiet = FALSE; //probably want to switch to TRUE for rphast after debugging
  pf->nratecats = 1;
  pf->use_em = FALSE;
  pf->window_size = -1;
  pf->window_shift = -1;
  pf->use_conditionals = FALSE;
  pf->precision = OPT_HIGH_PREC;
  pf->likelihood_only = FALSE;
  pf->do_bases = FALSE;
  pf->do_expected_nsubst = FALSE;
  pf->do_expected_nsubst_tot = FALSE;
  pf->random_init = FALSE;
  pf->estimate_backgd = FALSE;
  pf->estimate_scale_only = FALSE;
  pf->do_column_probs = FALSE;
  pf->nonoverlapping = FALSE;
  pf->gaps_as_bases = FALSE;
  pf->no_freqs = FALSE;
  pf->no_rates = FALSE;
  pf->assume_clock = FALSE;
  pf->init_parsimony = FALSE;
  pf->parsimony_only = FALSE;
  pf->no_branchlens = FALSE;
  pf->label_categories = TRUE;  //if false, assume MSA already has
                                //categories labelled with correct gff
  pf->nsites_threshold = DEFAULT_NSITES_THRESHOLD;
  pf->tree = NULL;
  pf->cm = NULL;
  pf->symfreq = FALSE;
  pf->nooptstr = NULL;
  pf->cats_to_do_str = NULL;
  pf->window_coords=NULL;
  pf->ignore_branches = NULL;
  pf->alt_mod_str = NULL;
  pf->bound_arg = NULL;
  pf->rate_consts = NULL;
  pf->alpha = DEFAULT_ALPHA;
  pf->gff = NULL;
  pf->input_mod = NULL;
  
  pf->estimated_models = NULL;
  pf->model_labels = NULL;
  return pf;
}

//print contents of phyloFit struct for testing
void print_phyloFit_struct(struct phyloFit_struct *pf) {
  printf("msa->length=%i\n", pf->msa->length);
  printf("msa->nseqs=%i\n", pf->msa->nseqs);
  printf("output_fname_root=%s\n", pf->output_fname_root==NULL ? "NULL" : pf->output_fname_root);
  printf("log_fname = %s\n", pf->log_fname == NULL ? "NULL" : pf->log_fname);
  printf("reverse_group_tag = %s\n", pf->reverse_group_tag == NULL ? "NULL" : pf->reverse_group_tag);
  printf("root_seqname = %s\n", pf->root_seqname == NULL ? "NULL" : pf->root_seqname);
  printf("subtree_name = %s\n", pf->subtree_name == NULL ? "NULL" : pf->subtree_name);
  printf("error_fname = %s\n", pf->error_fname == NULL ? "NULL" : pf->error_fname);
  printf("see_for_help = %s\n", pf->see_for_help == NULL ? "NULL" : pf->see_for_help);
  printf("parsimony_cost_fname = %s\n", pf->parsimony_cost_fname == NULL ? "NULL" : pf->parsimony_cost_fname);
  printf("msa_fname = %s\n", pf->msa_fname == NULL ? "NULL" : pf->msa_fname);
  printf("subst_mod = %i\n", pf->subst_mod);
  printf("quiet = %i\n", pf->quiet);
  printf("nratecats = %i\n", pf->nratecats);
  printf("use_em = %i\n", pf->use_em);
  printf("window_size = %i\n", pf->window_size);
  printf("window_shift = %i\n", pf->window_shift);
  printf("use_Conditionals=%i\n", pf->use_conditionals);
  printf("precision = %i\n", pf->precision);
  printf("likelihood_only = %i\n", pf->likelihood_only);
  printf("do_bases = %i\n", pf->do_bases);
  printf("do_expected_nsubst = %i\n", pf->do_expected_nsubst);
  printf("do_expected_nsubst_tot = %i\n", pf->do_expected_nsubst_tot);
  printf("random_init = %i\n", pf->random_init);
  printf("estimate_backgd = %i\n", pf->estimate_backgd);
  printf("estimate_scale_only=%i\n", pf->estimate_scale_only);
  printf("do_column_probs = %i\n", pf->do_column_probs);
  printf("nonoverlapping = %i\n", pf->nonoverlapping);
  printf("gaps_as_bases = %i\n", pf->gaps_as_bases);
  printf("no_freqs = %i\n", pf->no_freqs);
  printf("no_rates = %i\n", pf->no_rates);
  printf("assume_clock=%i\n", pf->assume_clock);
  printf("init_parsimony = %i\n", pf->init_parsimony);
  printf("parimony_only = %i\n", pf->parsimony_only);
  printf("no_branchlens = %i\n", pf->no_branchlens);
  printf("label_categories = %i\n", pf->label_categories);
  printf("nsites_threshold=%i\n", pf->nsites_threshold);
  printf("symfreq = %i\n", pf->symfreq);
  printf("tree: ");
  if (pf->tree != NULL)
    tr_print(stdout, pf->tree, 1);
  else printf("NULL\n");
  printf("cm: ");
  if (pf->cm == NULL) printf("is null\n");
  else cm_print(pf->cm, stdout);
  printf("nooptstr = %s\n", pf->nooptstr==NULL ? "NULL" : pf->nooptstr->chars);
  printf("cats_to_do==NULL=%i\n", pf->cats_to_do_str==NULL);
  printf("window_coords==NULL=%i\n", pf->window_coords==NULL);
  printf("ignore_branches==NULL=%i\n", pf->ignore_branches==NULL);
  printf("alt_mod_str==NULL=%i\n", pf->alt_mod_str==NULL);
  printf("bound_arg==NULL=%i\n", pf->bound_arg==NULL);
  printf("rate_consts==NULL=%i\n", pf->rate_consts==NULL);
  printf("alpha = %f\n", pf->alpha);
  printf("gff is NULL=%i\n", pf->gff==NULL);
  if (pf->input_mod != NULL)
    tm_print(stdout, pf->input_mod);
  else printf("NULL\n");

  printf("estimated_models==NULL=%i\n", pf->estimated_models==NULL);
  printf("model_labels == NULL=%i\n", pf->model_labels==NULL);
}




void set_output_fname(String *fname, char *root, int cat, char *suffix) {
  str_cpy_charstr(fname, root);
  if (cat != -1) {
    str_append_charstr(fname, ".");
    str_append_int(fname, cat);
  }
  str_append_charstr(fname, suffix);
}


/* Compute and output statistics based on posterior probabilities,
   including (optionally) the post prob of each tuple of bases at each
   ancestral node at each site (do_bases), the expected total number
   of substs per site (do_expected_nsubst), and the expected number of
   substitutions of each type on each edge across all sites
   (do_expected_nsubst_tot).  A separate file is output for each
   selected option, with an appropriate filename suffix (".postprob",
   ".expsub", and ".exptotsub", respectively).    */
void print_post_prob_stats(TreeModel *mod, MSA *msa, char *output_fname_root, 
                           int do_bases, int do_expected_nsubst, 
                           int do_expected_nsubst_tot, int cat, int quiet) {
  String *fname = str_new(STR_MED_LEN);
  FILE *POSTPROBF, *EXPSUBF, *EXPTOTSUBF;
  int i, tup, node, state, state2;
  TreeNode *n;
  char tuplestr[mod->order+2];
  char coltupstr[msa->nseqs+1];
  tuplestr[mod->order+1] = '\0';
  coltupstr[msa->nseqs] = '\0';

  /* FIXME: rate variation!     need rate post probs! */
  assert(mod->nratecats == 1);

  /* compute desired stats */
  assert(mod->tree_posteriors == NULL); 
  if (!quiet) 
    fprintf(stderr, "Computing posterior probabilities and/or related stats ...\n");
  mod->tree_posteriors = tl_new_tree_posteriors(mod, msa, do_bases, 0, 
                                                do_expected_nsubst, 
                                                do_expected_nsubst_tot, 0, 0);
  tl_compute_log_likelihood(mod, msa, NULL, cat, mod->tree_posteriors);

  if (do_bases) {
    set_output_fname(fname, output_fname_root, cat, ".postprob");
    if (!quiet) 
      fprintf(stderr, "Writing posterior probabilities to %s ...\n", 
              fname->chars);
    POSTPROBF = fopen_fname(fname->chars, "w+");

    /* print header */
    fprintf(POSTPROBF, "%-6s ", "#");
    for (i = 0; i < msa->nseqs; i++) fprintf(POSTPROBF, " ");
    fprintf(POSTPROBF, "    ");
    for (node = 0; node < mod->tree->nnodes; node++) {
      n = lst_get_ptr(mod->tree->nodes, node);
      if (n->lchild == NULL || n->rchild == NULL) continue;
      for (state = 0; state < mod->rate_matrix->size; state++) {
        if (state == mod->rate_matrix->size/2)
          fprintf(POSTPROBF, "node %-2d", n->id);
        else
          fprintf(POSTPROBF, "%6s ", "");
      }
    }
    fprintf(POSTPROBF, "\n%-6s ", "#");
    for (state = 0; state < msa->nseqs-5; state++) fprintf(POSTPROBF, " ");
    fprintf(POSTPROBF, "tuple     ");
    for (node = 0; node < mod->tree->nnodes; node++) {
      n = lst_get_ptr(mod->tree->nodes, node);
      if (n->lchild == NULL || n->rchild == NULL) continue;
      for (state = 0; state < mod->rate_matrix->size; state++) {
        get_tuple_str(tuplestr, state, mod->order + 1, 
                      mod->rate_matrix->states);
        fprintf(POSTPROBF, "%6s ", tuplestr);
      }
    }
    fprintf(POSTPROBF, "\n");

    /* print post probs */
    for (tup = 0; tup < msa->ss->ntuples; tup++) {

      if ((cat >= 0 && msa->ss->cat_counts[cat][tup] == 0) ||
          msa->ss->counts[tup] == 0) continue;

      tuple_to_string_pretty(coltupstr, msa, tup);
      fprintf(POSTPROBF, "%-6d %5s      ", tup, coltupstr);
      for (node = 0; node < mod->tree->nnodes; node++) {
        n = lst_get_ptr(mod->tree->nodes, node);
        if (n->lchild == NULL || n->rchild == NULL) continue;
        for (state = 0; state < mod->rate_matrix->size; state++) 
          fprintf(POSTPROBF, "%6.4f ", 
                  mod->tree_posteriors->base_probs[0][state][node][tup]);
      }                 
      fprintf(POSTPROBF, "\n");
    }
    fclose(POSTPROBF);
  }

  if (do_expected_nsubst) {
    set_output_fname(fname, output_fname_root, cat, ".expsub");
    if (!quiet) 
      fprintf(stderr, "Writing expected numbers of substitutions to %s ...\n", 
              fname->chars);
    EXPSUBF = fopen_fname(fname->chars, "w+");

    fprintf(EXPSUBF, "%-3s %10s %7s ", "#", "tuple", "count");
    for (node = 0; node < mod->tree->nnodes; node++) {
      n = lst_get_ptr(tr_postorder(mod->tree), node);
      if (n == mod->tree) continue;
      fprintf(EXPSUBF, " node_%-2d", n->id);
    }
    fprintf(EXPSUBF, "    total\n");
    for (tup = 0; tup < msa->ss->ntuples; tup++) {
      double total = 0;

      if ((cat >= 0 && msa->ss->cat_counts[cat][tup] == 0) ||
          msa->ss->counts[tup] == 0) continue;

      tuple_to_string_pretty(coltupstr, msa, tup);
      fprintf(EXPSUBF, "%-3d %10s %.0f ", tup, coltupstr, msa->ss->counts[tup]);
      for (node = 0; node < mod->tree->nnodes; node++) {
        n = lst_get_ptr(tr_postorder(mod->tree), node);
        if (n == mod->tree) continue;
        fprintf(EXPSUBF, "%7.4f ", 
                mod->tree_posteriors->expected_nsubst[0][n->id][tup]);
        total += mod->tree_posteriors->expected_nsubst[0][n->id][tup];
      }                 
      fprintf(EXPSUBF, "%7.4f\n", total);
    }
    fclose(EXPSUBF);
  }

  if (do_expected_nsubst_tot) {
    set_output_fname(fname, output_fname_root, cat, ".exptotsub");
    if (!quiet) 
      fprintf(stderr, "Writing total expected numbers of substitutions to %s ...\n", 
              fname->chars);
    EXPTOTSUBF = fopen_fname(fname->chars, "w+");

    fprintf(EXPTOTSUBF, "\n\
A separate matrix of expected numbers of substitutions is shown for each\n\
branch of the tree.     Nodes of the tree are visited in a postorder traversal,\n\
and each node is taken to be representative of the branch between itself and\n\
its parent.     Starting bases or tuples of bases appear on the vertical axis\n\
of each matrix, and destination bases or tuples of bases appear on the\n\
horizontal axis.\n\n");

    for (node = 0; node < mod->tree->nnodes; node++) {
      n = lst_get_ptr(tr_postorder(mod->tree), node);
      if (n == mod->tree) continue;

      fprintf(EXPTOTSUBF, "Branch above node %d", n->id);
      if (n->name != NULL && strlen(n->name) > 0) 
        fprintf(EXPTOTSUBF, " (leaf labeled '%s')", n->name);
      fprintf(EXPTOTSUBF, ":\n\n");

      /* print header */
      fprintf(EXPTOTSUBF, "%-4s ", "");
      for (state2 = 0; state2 < mod->rate_matrix->size; state2++) {
        get_tuple_str(tuplestr, state2, mod->order + 1, 
                      mod->rate_matrix->states);
        fprintf(EXPTOTSUBF, "%12s ", tuplestr);
      }
      fprintf(EXPTOTSUBF, "\n");
      for (state = 0; state < mod->rate_matrix->size; state++) {
        get_tuple_str(tuplestr, state, mod->order + 1, 
                      mod->rate_matrix->states);
        fprintf(EXPTOTSUBF, "%-4s ", tuplestr);
        for (state2 = 0; state2 < mod->rate_matrix->size; state2++) 
          fprintf(EXPTOTSUBF, "%12.2f ", 
                  mod->tree_posteriors->expected_nsubst_tot[0][state][state2][n->id]);
        fprintf(EXPTOTSUBF, "\n");
      }
      fprintf(EXPTOTSUBF, "\n\n");
    }
    fclose(EXPTOTSUBF);
  }

  tl_free_tree_posteriors(mod, msa, mod->tree_posteriors);
  mod->tree_posteriors = NULL;
  str_free(fname);
}


void print_window_summary(FILE* WINDOWF, List *window_coords, int win, 
                          int cat, TreeModel *mod, double *gc, double cpg, 
                          int ninf_sites, int nseqs, int header_only) {
  int j;
  if (header_only) {
    fprintf(WINDOWF, "%5s %8s %8s %4s", "win", "beg", "end", "cat");
    fprintf(WINDOWF, " %6s", "GC");
    fprintf(WINDOWF, " %8s", "CpG");
    fprintf(WINDOWF, " %7s", "ninf");
    fprintf(WINDOWF, " %7s\n", "t");
  }
  else {
    fprintf(WINDOWF, "%5d %8d %8d %4d", win/2+1, 
            lst_get_int(window_coords, win), 
            lst_get_int(window_coords, win+1), cat);
    fprintf(WINDOWF, " %6.4f", 
            vec_get(mod->backgd_freqs, 
                           mod->rate_matrix->inv_states[(int)'G']) + 
            vec_get(mod->backgd_freqs, 
                           mod->rate_matrix->inv_states[(int)'C']));
    for (j = 0; j < nseqs; j++) fprintf(WINDOWF, " %6.4f", gc[j]);
    fprintf(WINDOWF, " %8.6f", cpg);
    fprintf(WINDOWF, " %7d", ninf_sites);
    fprintf(WINDOWF, " %7.4f\n", tr_total_len(mod->tree));
  }
}



int run_phyloFit(struct phyloFit_struct *pf) {
  FILE *F, *WINDOWF;
  int i, j, win, root_leaf_id = -1;
  String *mod_fname;
  MSA *source_msa;
  FILE *logf = NULL;
  String *tmpstr = str_new(STR_SHORT_LEN);
  List *cats_to_do=NULL;
  double *gc;  //TODO: gc and cpg don't seem to have been implemented
  double cpg;
  char tmpchstr[STR_MED_LEN];
  FILE *parsimony_cost_file = NULL;
  int free_cm = FALSE, free_cats_to_do_str=FALSE, free_tree=FALSE,
    free_window_coords = FALSE;

  //copy some heavily used variables directly from pf for easy access
  MSA *msa = pf->msa;
  int subst_mod = pf->subst_mod;
  TreeNode *tree = pf->tree;
  GFF_Set *gff = pf->gff;
  int quiet = pf->quiet;
  TreeModel *input_mod = pf->input_mod;


  
  //    print_phyloFit_struct(pf);

  if (pf->log_fname != NULL) {
    if (!strcmp(pf->log_fname, "-"))
      logf = stderr;
    else
      logf = fopen_fname(pf->log_fname, "w+");
  }
  if (pf->parsimony_cost_fname != NULL)
    parsimony_cost_file = fopen_fname(pf->parsimony_cost_fname, "w");

  if (pf->use_conditionals && pf->use_em) 
    die("ERROR: Cannot use --markov with --EM.    Type %s for usage.\n",
	pf->see_for_help);

  if (pf->likelihood_only && input_mod == NULL)  
    die("ERROR: --lnl requires --init-model.  Type '%s' for usage.\n",
	pf->see_for_help);

  if (input_mod != NULL && tree != NULL)
    die("ERROR: --tree is not allowed with --init-model.\n");

  if (pf->gaps_as_bases && subst_mod != JC69 && subst_mod != F81 && 
      subst_mod != HKY85G && subst_mod != REV && 
      subst_mod != UNREST && subst_mod != SSREV)
    die("ERROR: --gaps-as-bases currently only supported with JC69, F81, HKY85+Gap, REV, SSREV, and UNREST.\n");
                                /* with HKY, yields undiagonalizable matrix */
  if ((pf->no_freqs || pf->no_rates) && input_mod == NULL)
    die("ERROR: --init-model required with --no-freqs and/or --no-rates.\n");

  if (pf->no_freqs && pf->estimate_backgd)
    die("ERROR: can't use both --no-freqs and --estimate-freqs.\n");

  if (gff != NULL && pf->cm == NULL) {
    pf->cm = cm_new_from_features(gff);
    free_cm = TRUE;
  }

  if (pf->subtree_name != NULL && pf->estimate_scale_only == FALSE) {
    if (!quiet) 
      fprintf(stderr, "warning: specifying subtree implies scale_only\n");
    pf->estimate_scale_only = TRUE;
  }

  if (pf->rate_consts != NULL) {
    lst_qsort_dbl(pf->rate_consts, ASCENDING);
    if (lst_size(pf->rate_consts) < 2 || lst_get_dbl(pf->rate_consts, 0) <= 0) 
      die("ERROR: must be >= 2 rate constants and all must be positive.\n");
    if (pf->nratecats != lst_size(pf->rate_consts))
      die("ERROR: rate_consts must have length nratecats");
  }

  /* internally, --non-overlapping is accomplished via --do-cats */
  if (pf->nonoverlapping) {
    if (pf->cats_to_do_str != NULL)
      die("ERROR: cannot use --do-cats with nonoverlapping");
    pf->cats_to_do_str = lst_new_ptr(1);
    lst_push_ptr(pf->cats_to_do_str, str_new_charstr("1"));
    free_cats_to_do_str = TRUE;
  }

  if (tree == NULL) {
    if (input_mod != NULL) tree = input_mod->tree;
    else if (msa->nseqs == 2) {
      sprintf(tmpchstr, "(%s,%s)", msa->names[0], msa->names[1]);
      tree = tr_new_from_string(tmpchstr);
      free_tree = TRUE;
    }
    else if (msa->nseqs == 3 && tm_is_reversible(subst_mod)) {
      sprintf(tmpchstr, "(%s,(%s,%s))", msa->names[0], msa->names[1], 
              msa->names[2]);
      tree = tr_new_from_string(tmpchstr);
      free_tree = TRUE;
    }
    else die("ERROR: --tree required.\n");
  }


  /* allow for specified ancestor */
  if (pf->root_seqname != NULL) {
    TreeNode *rl;
    if (tree == NULL || tm_is_reversible(subst_mod)) 
      die("ERROR: --ancestor requires --tree and a non-reversible model.\n");
    rl = tr_get_node(tree, pf->root_seqname);     
    if (rl == NULL || rl->parent != tree) 
      die("ERROR: Sequence specified by --ancestor must be a child of the root.\n");
    root_leaf_id = rl->id;
  }
  
  if (msa_alph_has_lowercase(msa)) msa_toupper(msa); 
  msa_remove_N_from_alph(msa);    /* for backward compatibility */

  /* set up for categories */
  /* first label sites, if necessary */
  if (gff != NULL && pf->label_categories) {
    if (msa->idx_offset > 0) {
      /* if these's an offset, we'll just subtract it from all features */
      for (i = 0; i < lst_size(gff->features); i++) {
        GFF_Feature *f = lst_get_ptr(gff->features, i);
        f->start -= msa->idx_offset;
        f->end -= msa->idx_offset;
      }
      msa->idx_offset = 0;
    }

    /* convert GFF to coordinate frame of alignment */
    msa_map_gff_coords(msa, gff, 1, 0, 0, NULL);

    /* reverse complement segments of MSA corresponding to features on
       reverse strand (if necessary) */
    if (pf->reverse_group_tag != NULL) {
      gff_group(gff, pf->reverse_group_tag);
      msa_reverse_compl_feats(msa, gff, NULL);
    }
    
    /* label categories */
    if (!quiet) fprintf(stderr, "Labeling alignment sites by category ...\n");
    msa_label_categories(msa, gff, pf->cm);
  }
  else if (pf->nonoverlapping && pf->label_categories) {
                                /* (already taken care of if MAF) */
    int cycle_size = tm_order(subst_mod) + 1;
    assert(msa->seqs != NULL && msa->ss == NULL);  /* need explicit seqs */
    msa->categories = smalloc(msa->length * sizeof(int));
    for (i = 0; i < msa->length; i++) 
      msa->categories[i] = (i % cycle_size) + 1;
    msa->ncats = cycle_size;
  }
  /* at this point, we have msa->ncats > 0 iff we intend to do
     category-by-category estimation */

  /* now set up list of categories to process.    There are several
     cases to consider */
  if (msa->ncats < 0) {            
    if (pf->cats_to_do_str != NULL)
      fprintf(stderr, "WARNING: ignoring --do-cats; no category information.\n");
    cats_to_do = lst_new_int(1);
    lst_push_int(cats_to_do, -1);
    /* no categories -- pool all sites */
  }
  else if (pf->cats_to_do_str == NULL) {
    cats_to_do = lst_new_int(msa->ncats + 1);
    for (i = 0; i <= msa->ncats; i++) lst_push_int(cats_to_do, i);
                                /* have categories but no --do-cats --
                                   process all categories */
  }
  else if (pf->cm != NULL) 
    cats_to_do = cm_get_category_list(pf->cm, pf->cats_to_do_str, 0);
                                /* have --do-cats and category map;
                                   use cm_get_category_list (allows
                                   use of names as well as numbers) */
  else if (pf->cats_to_do_str != NULL)
    cats_to_do = str_list_as_int(pf->cats_to_do_str);
                                /* have --do-cats but no category map;
                                   use literal numbers */
  /* set up windows, if necessary */
  if (pf->window_size != -1) {
    if (pf->window_coords != NULL) 
      die("ERROR: cannot use both --windows and --windows-explicit.\n");
    pf->window_coords = lst_new_int(msa->length/pf->window_shift + 1);
    for (i = 1; i < msa->length; i += pf->window_shift) {
      lst_push_int(pf->window_coords, i);
      lst_push_int(pf->window_coords, 
                   min(i + pf->window_size - 1, msa->length));
    }
    free_window_coords = TRUE;
  }
  if (pf->window_coords != NULL) {
    /* set up summary file */
    String *sumfname;
    msa_coord_map *map;

    if (pf->output_fname_root == NULL) 
      die("Currently need to specify output file to run phyloFit in windows");
    sumfname = str_new_charstr(pf->output_fname_root);
    str_append_charstr(sumfname, ".win-sum");
    WINDOWF = fopen_fname(sumfname->chars, "w+");
    str_free(sumfname);
    print_window_summary(WINDOWF, NULL, 0, 0, NULL, NULL, 0, 0, 0, TRUE);
    
    /* map to coord frame of alignment */
    map = msa_build_coord_map(msa, 1);
    for (i = 0; i < lst_size(pf->window_coords); i += 2) {
      lst_set_int(pf->window_coords, i, 
                  msa_map_seq_to_msa(map, lst_get_int(pf->window_coords, i)));
      lst_set_int(pf->window_coords, i+1, 
                  msa_map_seq_to_msa(map, lst_get_int(pf->window_coords, i+1)));
    }
    msa_map_free(map);
  }
  
  /* now estimate models (window by window, if necessary) */
  mod_fname = str_new(STR_MED_LEN);
  source_msa = msa;
  for (win = 0; 
       win < (pf->window_coords == NULL ? 1 : lst_size(pf->window_coords)); 
       win += 2) {
    int win_beg, win_end;

    if (pf->window_coords != NULL) {
      win_beg = lst_get_int(pf->window_coords, win);
      win_end = lst_get_int(pf->window_coords, win+1);
      if (win_beg < 0 || win_end < 0) continue;
      
      /* note: msa_sub_alignment uses a funny indexing system (see docs) */
      msa = msa_sub_alignment(source_msa, NULL, 0, win_beg-1, win_end);
    }

    /* process each category */
    for (i = 0; i < lst_size(cats_to_do); i++) {
      TreeModel *mod;
      Vector *params = NULL;
      List *pruned_names;
      int old_nnodes, cat = lst_get_int(cats_to_do, i);
      unsigned int ninf_sites;

      if (input_mod == NULL) 
        mod = tm_new(tr_create_copy(tree), NULL, NULL, subst_mod, 
                     msa->alphabet, pf->nratecats, pf->alpha, 
		     pf->rate_consts, root_leaf_id);
      else if (pf->likelihood_only)
        mod = input_mod;
      else {
        double newalpha = 
          (input_mod->nratecats > 1 && pf->alpha == DEFAULT_ALPHA ? 
           input_mod->alpha : pf->alpha);
                                /* if the input_mod has a meaningful
                                   alpha and a non-default alpha has
                                   not been specified, then use
                                   input_mod's alpha  */
        mod = input_mod;
        tm_reinit(mod, subst_mod, pf->nratecats, newalpha, 
		  pf->rate_consts, NULL);
      }

      mod->noopt_arg = pf->nooptstr;
      mod->eqfreq_sym = pf->symfreq;
      mod->bound_arg = pf->bound_arg;

      mod->use_conditionals = pf->use_conditionals;

      if (pf->estimate_scale_only || 
	  pf->estimate_backgd || 
	  pf->no_rates || 
	  pf->assume_clock) {
        if (pf->estimate_scale_only) {
          mod->estimate_branchlens = TM_SCALE_ONLY;

          if (pf->subtree_name != NULL) { /* estimation of subtree scale */
            String *s1 = str_new_charstr(pf->subtree_name), 
              *s2 = str_new_charstr(pf->subtree_name);
            str_root(s1, ':'); str_suffix(s2, ':'); /* parse string */
            mod->subtree_root = tr_get_node(mod->tree, s1->chars);
            if (mod->subtree_root == NULL)
              die("ERROR: no node named '%s'.\n", s1->chars);
            if (s2->length > 0) {
              if (str_equals_charstr(s2, "loss")) 
		mod->scale_sub_bound = LB;
              else if (str_equals_charstr(s2, "gain")) 
		mod->scale_sub_bound = UB;
              else die("ERROR: unrecognized suffix '%s'\n", s2->chars);
            }
            str_free(s1); str_free(s2);
          }
        }
	
        else if (pf->assume_clock)
          mod->estimate_branchlens = TM_BRANCHLENS_CLOCK;
        
        if (pf->no_rates)
          mod->estimate_ratemat = FALSE;

        mod->estimate_backgd = pf->estimate_backgd;
      }
      
      if (pf->no_branchlens)
	mod->estimate_branchlens = TM_BRANCHLENS_NONE;

      if (pf->ignore_branches != NULL) 
        tm_set_ignore_branches(mod, pf->ignore_branches);

      old_nnodes = mod->tree->nnodes;
      pruned_names = lst_new_ptr(msa->nseqs);
      tm_prune(mod, msa, pruned_names);
      if (lst_size(pruned_names) == (old_nnodes + 1) / 2)
        die("ERROR: no match for leaves of tree in alignment (leaf names must match alignment names).\n");
      if (!quiet && lst_size(pruned_names) > 0) {
        fprintf(stderr, "WARNING: pruned away leaves of tree with no match in alignment (");
        for (j = 0; j < lst_size(pruned_names); j++)
          fprintf(stderr, "%s%s", ((String*)lst_get_ptr(pruned_names, j))->chars, 
                  j < lst_size(pruned_names) - 1 ? ", " : ").\n");
      }
      lst_free_strings(pruned_names);
      lst_free(pruned_names);

      if (pf->alt_mod_str != NULL) {
	for (j = 0 ; j < lst_size(pf->alt_mod_str); j++) 
	  tm_add_alt_mod(mod, (String*)lst_get_ptr(pf->alt_mod_str, j));
      }

      str_clear(tmpstr);
      
      if  (pf->msa_fname != NULL)
	str_append_charstr(tmpstr, pf->msa_fname);
      else str_append_charstr(tmpstr, "alignment");
      
      if (cat != -1 || pf->window_coords != NULL) {
	str_append_charstr(tmpstr, " (");
	if (cat != -1) {
	  str_append_charstr(tmpstr, "category ");
	  str_append_int(tmpstr, cat);
	}
	
	if (pf->window_coords != NULL) {
	  if (cat != -1) str_append_charstr(tmpstr, ", ");
	  str_append_charstr(tmpstr, "window ");
	  str_append_int(tmpstr, win/2 + 1);
	}
	
	str_append_char(tmpstr, ')');
      }

      ninf_sites = msa_ninformative_sites(msa, cat);
      if (ninf_sites < pf->nsites_threshold) {
        if (input_mod == NULL) tm_free(mod);
        fprintf(stderr, "Skipping %s; insufficient informative sites ...\n", 
                tmpstr->chars);
        continue;
      }

      if (pf->init_parsimony) {
	double parsimony_cost = tm_params_init_branchlens_parsimony(NULL, mod, msa, cat);
        if (parsimony_cost_file != NULL) 
           fprintf(parsimony_cost_file, "%f\n", parsimony_cost);
        if (pf->parsimony_only) continue;
      }

      if (pf->likelihood_only) {
        double *col_log_probs = pf->do_column_probs ? 
          smalloc(msa->length * sizeof(double)) : NULL;
        String *colprob_fname;
        if (!quiet) 
          fprintf(stderr, "Computing likelihood of %s ...\n", tmpstr->chars);
        tm_set_subst_matrices(mod);
        if (pf->do_column_probs && msa->ss != NULL && msa->ss->tuple_idx == NULL) {
          msa->ss->tuple_idx = smalloc(msa->length * sizeof(int));
          for (j = 0; j < msa->length; j++)
            msa->ss->tuple_idx[j] = j;
        }
        mod->lnL = tl_compute_log_likelihood(mod, msa, col_log_probs, cat, NULL) * 
          log(2);
        if (pf->do_column_probs) {
	  //we don't need to implement this in RPHAST because there is
	  //already a msa.likelihood function
	  if (pf->output_fname_root == NULL)
	    die("ERROR: currently do_column_probs requires output file");
          colprob_fname = str_new_charstr(pf->output_fname_root);
          str_append_charstr(colprob_fname, ".colprobs");
          if (!quiet) 
            fprintf(stderr, "Writing column probabilities to %s ...\n", 
                    colprob_fname->chars);
          F = fopen_fname(colprob_fname->chars, "w+");
          for (j = 0; j < msa->length; j++)
            fprintf(F, "%d\t%.6f\n", j, col_log_probs[j]);
          fclose(F);
          str_free(colprob_fname);
          free(col_log_probs);
        }
      }
      else {                    /* fit model */

        if (msa->ss == NULL) {    /* get sufficient stats if necessary */
          if (!quiet)
            fprintf(stderr, "Extracting sufficient statistics ...\n");
          ss_from_msas(msa, mod->order+1, 0, 
                       pf->cats_to_do_str != NULL ? cats_to_do : NULL, 
                       NULL, NULL, -1);
          /* (sufficient stats obtained only for categories of interest) */
      
          if (msa->length > 1000000) { /* throw out original data if
                                          very large */
            for (j = 0; j < msa->nseqs; j++) free(msa->seqs[j]);
            free(msa->seqs);
            msa->seqs = NULL;
          }
        }

        if (pf->random_init) 
          params = tm_params_init_random(mod);
        else if (input_mod != NULL) 
          params = tm_params_new_init_from_model(input_mod);
	else 
          params = tm_params_init(mod, .1, 5, pf->alpha);     

	if (pf->init_parsimony)
	  tm_params_init_branchlens_parsimony(params, mod, msa, cat);

        if (input_mod != NULL && mod->backgd_freqs != NULL && !pf->no_freqs) {
          /* in some cases, the eq freqs are needed for
             initialization, but now they should be re-estimated --
             UNLESS user specifies --no-freqs */
          vec_free(mod->backgd_freqs);
          mod->backgd_freqs = NULL;
        }


        if (i == 0) {
          if (!quiet) fprintf(stderr, "Compacting sufficient statistics ...\n");
          ss_collapse_missing(msa, !pf->gaps_as_bases);
                                /* reduce number of tuples as much as
                                   possible */
        }

        if (!quiet) {
          fprintf(stderr, "Fitting tree model to %s using %s%s ...\n",
                  tmpstr->chars, tm_get_subst_mod_string(subst_mod),
                  mod->nratecats > 1 ? " (with rate variation)" : "");
          if (pf->log_fname != NULL)
            fprintf(stderr, "(writing log to %s)\n", pf->log_fname);
        }

        if (pf->use_em)
          tm_fit_em(mod, msa, params, cat, pf->precision, logf);
        else
          tm_fit(mod, msa, params, cat, pf->precision, logf);

        if (pf->error_fname != NULL)
	  tm_variance(mod, msa, params, cat, pf->error_fname, i!=0 || win!=0);

      }  

      if (pf->output_fname_root != NULL) 
	str_cpy_charstr(mod_fname, pf->output_fname_root);
      else str_clear(mod_fname);
      if (pf->window_coords != NULL) {
	if (mod_fname->length != 0) 
	  str_append_char(mod_fname, '.');
        str_append_charstr(mod_fname, "win-");
        str_append_int(mod_fname, win/2 + 1);
      }
      if (cat != -1 && pf->nonoverlapping == FALSE) {
	if (mod_fname->length != 0) 
	  str_append_char(mod_fname, '.');
        if (pf->cm != NULL)  
          str_append(mod_fname, cm_get_feature_unique(pf->cm, cat));
        else 
          str_append_int(mod_fname, cat);
      }
      if (pf->output_fname_root != NULL)
	str_append_charstr(mod_fname, ".mod");

      if (pf->output_fname_root != NULL) {
	if (!quiet) fprintf(stderr, "Writing model to %s ...\n", 
			    mod_fname->chars);
	F = fopen_fname(mod_fname->chars, "w+");
	tm_print(F, mod);
	fclose(F);
      }
      if (pf->estimated_models != NULL) {
	lst_push_ptr(pf->estimated_models, (void*)tm_create_copy(mod));
      }
      if (pf->model_labels != NULL) {
	lst_push_ptr(pf->model_labels, (void*)str_new_charstr(mod_fname->chars));
      }

      /* output posterior probabilities, if necessary */
      if (pf->output_fname_root != NULL) {
	if (pf->do_bases || pf->do_expected_nsubst || pf->do_expected_nsubst_tot) 
	  print_post_prob_stats(mod, msa, pf->output_fname_root, 
				pf->do_bases, pf->do_expected_nsubst, 
				pf->do_expected_nsubst_tot, 
				cat, quiet);
      }

      /* print window summary, if window mode */
      if (pf->window_coords != NULL) 
        print_window_summary(WINDOWF, pf->window_coords, win, cat, mod, gc, 
                             cpg, ninf_sites, msa->nseqs, FALSE);

      if (input_mod == NULL) tm_free(mod);
      if (params != NULL) vec_free(params);
    }
    if (pf->window_coords != NULL) 
      msa_free(msa);
  }
  if (parsimony_cost_file != NULL) fclose(parsimony_cost_file); 
  free(mod_fname);
  str_free(tmpstr);
  if (free_cm) {
    cm_free(pf->cm);
    pf->cm = NULL;
  }
  if (free_cats_to_do_str) {
    lst_free_strings(pf->cats_to_do_str);
    lst_free(pf->cats_to_do_str);
    pf->cats_to_do_str = NULL;
  }
  if (free_tree) 
    tr_free(tree);
  if (cats_to_do != NULL) lst_free(cats_to_do);
  if (free_window_coords) {
    lst_free(pf->window_coords);
    pf->window_coords = NULL;
  }
  return 0;
}