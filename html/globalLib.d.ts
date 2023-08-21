import type * as moment from 'moment';
import type * as _ from 'lodash';
import type {Vue as VueType} from 'vue/types/vue.d.ts';

export {};

declare global {
    const moment: typeof moment;
    // const Vue: any;
    const _: typeof _;

    interface Window {
        i18nTable: { [key: string]: { s: string, f?: CallableFunction } };
        i18n: {
            [key: string]: { [key: string]: { s: string, f?: CallableFunction } }
        };
    }
}
