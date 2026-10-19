# LEKAK iPhone - règles de fusion v1.4 (port iOS)

Cette branche ajoute les deux règles du pack Windows validé :
- Résultat de fusion = variante la plus faible des deux cartes (normale = niveau 0).
- Blocage si l'ATK du résultat, bonus de rareté compris, est inférieure à l'ATK de l'un des deux matériaux.

Le code est intégré à la chaîne native ARM64 à travers l'overlay `Updates/v35`. Les protections sont placées dans `SourceMod/Lekak/fusion-protection-ios.c` après extraction des sources. Aucune correction d'invisibilité des cartes adverses n'est ajoutée à l'iPhone.

Le patch utilisateur final nécessitera également de conserver les `copy` déjà synchronisés dans les mod.json iPhone et une validation sur appareil. Les tests du module C sur Linux passent (8/8), mais ne prouvent pas à eux seuls son comportement en duel iPhone.

Mise à jour : intégration additive (préserve le manifeste static_modules de v0.32).
